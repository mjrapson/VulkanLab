// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include <assets/asset_database.h>
#include <assets/gltf_loader.h>
#include <assets/image_loader.h>
#include <core/file_system.h>
#include <core/input_handler.h>
#include <renderer/camera.h>
#include <renderer/gpu_device.h>
#include <renderer/renderer.h>
#include <scene/scene.h>
#include <scene/scene_loader.h>
#include <world/systems/render_system.h>
#include <world/world.h>

#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <ranges>
#include <stdexcept>
#include <vector>

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = static_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    app->windowResized(width, height);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto app = static_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    app->keyPressed(key, scancode, action, mods);
}

constexpr bool validationLayersEnabled()
{
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

[[nodiscard]]
bool validateExtensions(const std::vector<const char*>& requiredExtensions, const vk::raii::Context& context)
{
    bool allExtensionsValid = true;

    const auto availableExtensions = context.enumerateInstanceExtensionProperties();
    for (const auto requiredExtension : requiredExtensions)
    {
        if (std::ranges::none_of(availableExtensions,
                                 [requiredExtension](const auto& extensionProperty)
                                 {
                                     return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                                 }))
        {
            allExtensionsValid = false;
            spdlog::error("Required extension {} not supported", requiredExtension);
        }
    }

    return allExtensionsValid;
}

[[nodiscard]]
bool validateLayers(const std::vector<const char*>& requiredLayers, const vk::raii::Context& context)
{
    bool allLayersValid = true;

    const auto availableLayers = context.enumerateInstanceLayerProperties();
    for (const auto& requiredLayer : requiredLayers)
    {
        if (std::ranges::none_of(availableLayers,
                                 [requiredLayer](const auto& layerProperty)
                                 {
                                     return strcmp(layerProperty.layerName, requiredLayer) == 0;
                                 }))
        {
            allLayersValid = false;
            spdlog::error("Required validation layer {} not available", requiredLayer);
        }
    }

    return allLayersValid;
}

static vk::Bool32 debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                vk::DebugUtilsMessageTypeFlagsEXT,
                                const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                void*)
{
    if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
    {
        spdlog::error("{}", pCallbackData->pMessage);
    }
    else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
    {
        spdlog::warn("{}", pCallbackData->pMessage);
    }
    else
    {
        spdlog::info("{}", pCallbackData->pMessage);
    }

    return VK_TRUE;
}

VulkanApplication::VulkanApplication()
    : inputHandler_{std::make_unique<core::InputHandler>()}
{
}

VulkanApplication::~VulkanApplication()
{
    if (window_)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        spdlog::info("GLFW Window destroyed");
    }

    if (glfwInitialised_)
    {
        glfwTerminate();
        spdlog::info("GLFW terminated");
    }
}

void VulkanApplication::init(int windowWidth, int windowHeight, const std::string& windowTitle)
{
    spdlog::info("Initializing GLFW");
    initGlfw();

    spdlog::info("Initializing GLFW window");
    initWindow(windowWidth, windowHeight, windowTitle);

    spdlog::info("Initializing Vulkan");
    initVulkan(windowWidth, windowHeight);
}

void VulkanApplication::run()
{
    spdlog::info("Running");

    auto scene = scene::loadScene(core::getScenesDir() / "demo.json");
    camera_->setPosition(glm::vec3{0.0f, 8.0f, 24.0f});

    // Probably show some loading screen here...
    // Move to separate func
    auto db = assets::AssetDatabase{};
    for (auto& prefabDef : scene->prefabs)
    {
        db.addPrefab(prefabDef.name, assets::loadGLTFModel(core::getPrefabsDir() / prefabDef.path));
    }

    for (auto& skyboxDef : scene->skyboxes)
    {
        auto skybox = std::make_unique<assets::Skybox>();
        skybox->px = assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.pxPath);
        skybox->py = assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.pyPath);
        skybox->pz = assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.pzPath);
        skybox->nx = assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.nxPath);
        skybox->ny = assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.nyPath);
        skybox->nz = assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.nzPath);

        db.addSkybox(skyboxDef.name, std::move(skybox));
    }

    renderer_->setResources(db);

    auto world = world::World{*scene, db, *renderer_};
    // ...end loading screen

    constexpr auto maxFps = std::chrono::duration<double>(1.0 / 60.0);
    auto lastTime = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window_))
    {
        const auto frameStartTime = std::chrono::steady_clock::now();
        const auto deltaTime = std::chrono::duration<double>(frameStartTime - lastTime).count();
        lastTime = frameStartTime;

        glfwPollEvents();

        updateCamera(deltaTime);

        world.update(*camera_);

        const auto frameFinishTime = std::chrono::steady_clock::now();
        const auto frameDuration = frameFinishTime - frameStartTime;
        if (frameDuration < maxFps)
        {
            std::this_thread::sleep_for(maxFps - frameDuration);
        }
    }

    gpuDevice_->device().waitIdle();
}

void VulkanApplication::windowResized(int width, int height)
{
    renderer_->windowResized(width, height);

    const auto aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    camera_->setAspectRatio(aspectRatio);
}

void VulkanApplication::keyPressed(int key, int, int action, int)
{
    if (action == GLFW_PRESS)
    {
        inputHandler_->setKeyPressed(key);
    }
    else if (action == GLFW_RELEASE)
    {
        inputHandler_->setKeyReleased(key);
    }
}

void VulkanApplication::initGlfw()
{
    glfwSetErrorCallback(
        [](int errorCode, const char* description)
        {
            spdlog::error("GLFW error {}: {}", errorCode, description);
        });

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwInitialised_ = true;
}

void VulkanApplication::initWindow(int windowWidth, int windowHeight, const std::string& windowTitle)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(windowWidth, windowHeight, windowTitle.c_str(), nullptr, nullptr);
    if (!window_)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetKeyCallback(window_, keyCallback);
}

void VulkanApplication::initVulkan(int windowWidth, int windowHeight)
{
    const auto version = vk::enumerateInstanceVersion();
    spdlog::info("Vulkan API version: {}.{}.{}",
                 VK_VERSION_MAJOR(version),
                 VK_VERSION_MINOR(version),
                 VK_VERSION_PATCH(version));

    spdlog::info("Creating Vulkan instance");
    createInstance();

    if (validationLayersEnabled())
    {
        spdlog::info("Setting up Vulkan debug messaging");
        createDebugMessenger();
    }

    spdlog::info("Creating window surface");
    createSurface();

    spdlog::info("Creating GPU device");
    gpuDevice_ = std::make_unique<renderer::GpuDevice>(instance_, surface_);

    spdlog::info("Creating renderer");
    renderer_ = std::make_unique<renderer::Renderer>(instance_, surface_, *gpuDevice_, windowWidth, windowHeight);

    spdlog::info("Creating camera");
    camera_ = std::make_unique<renderer::Camera>();
    const auto aspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    camera_->setAspectRatio(aspectRatio);
}

void VulkanApplication::createInstance()
{
    auto glfwExtensionCount = uint32_t{0};
    const auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto extensions = std::vector<const char*>{glfwExtensions, glfwExtensions + glfwExtensionCount};
    if (validationLayersEnabled())
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    const auto validationLayers = std::vector<char const*>{"VK_LAYER_KHRONOS_validation"};

    auto appInfo = vk::ApplicationInfo{};
    appInfo.pApplicationName = "Vulkan Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Vulkan Demo Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = vk::ApiVersion14;

    if (!validateExtensions(extensions, context_))
    {
        throw std::runtime_error("Requested extensions not available");
    }

    if (!validateLayers(validationLayers, context_))
    {
        throw std::runtime_error("Requested validation layers not available");
    }

    auto createInfo = vk::InstanceCreateInfo{};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    instance_ = vk::raii::Instance(context_, createInfo);
}

void VulkanApplication::createDebugMessenger()
{
    const auto severityFlags = (vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    const auto messageTypeFlags = (vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                   | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                                   | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    auto debugCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{};
    debugCreateInfo.messageSeverity = severityFlags;
    debugCreateInfo.messageType = messageTypeFlags;
    debugCreateInfo.pfnUserCallback = &debugCallback;

    debugMessenger_ = instance_.createDebugUtilsMessengerEXT(debugCreateInfo);
}

void VulkanApplication::createSurface()
{
    auto surface = VkSurfaceKHR{VK_NULL_HANDLE};
    if (glfwCreateWindowSurface(*instance_, window_, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface");
    }

    surface_ = vk::raii::SurfaceKHR(instance_, surface);
}

void VulkanApplication::updateCamera(float deltaTime)
{
    const auto speed = 15.0f;
    const auto turnSpeed = 45.0f;

    auto movement = glm::vec3{0.0f};

    auto worldUp = glm::vec3(0, 1, 0);
    auto forward = glm::normalize(camera_->front());
    auto right = glm::normalize(glm::cross(forward, worldUp));
    auto up = glm::normalize(glm::cross(right, forward));

    if (inputHandler_->isKeyPressed(GLFW_KEY_W))
    {
        movement = movement + forward;
    }

    if (inputHandler_->isKeyPressed(GLFW_KEY_S))
    {
        movement = movement - forward;
    }

    if (inputHandler_->isKeyPressed(GLFW_KEY_A))
    {
        camera_->setYaw(camera_->yaw() - (turnSpeed * deltaTime));
    }

    if (inputHandler_->isKeyPressed(GLFW_KEY_D))
    {
        camera_->setYaw(camera_->yaw() + (turnSpeed * deltaTime));
    }

    if (inputHandler_->isKeyPressed(GLFW_KEY_E))
    {
        movement = movement + up;
    }

    if (inputHandler_->isKeyPressed(GLFW_KEY_Q))
    {
        movement = movement - up;
    }

    if (glm::length(movement) > 0.0f)
    {
        movement = glm::normalize(movement) * speed * deltaTime;
        camera_->setPosition(camera_->position() + movement);
    }
}
