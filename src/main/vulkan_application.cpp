// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include <renderer/gpu_device.h>
#include <renderer/renderer.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <ranges>
#include <stdexcept>
#include <vector>

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = static_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    app->windowResized(width, height);
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
bool validateExtensions(const std::vector<const char*>& requiredExtensions,
                        const vk::raii::Context& context)
{
    bool allExtensionsValid = true;

    const auto availableExtensions = context.enumerateInstanceExtensionProperties();
    for (const auto requiredExtension : requiredExtensions)
    {
        if (std::ranges::none_of(
                availableExtensions, [requiredExtension](const auto& extensionProperty)
                { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; }))
        {
            allExtensionsValid = false;
            spdlog::error("Required extension {} not supported", requiredExtension);
        }
    }

    return allExtensionsValid;
}

[[nodiscard]]
bool validateLayers(const std::vector<const char*>& requiredLayers,
                    const vk::raii::Context& context)
{
    bool allLayersValid = true;

    const auto availableLayers = context.enumerateInstanceLayerProperties();
    for (const auto& requiredLayer : requiredLayers)
    {
        if (std::ranges::none_of(availableLayers, [requiredLayer](const auto& layerProperty)
                                 { return strcmp(layerProperty.layerName, requiredLayer) == 0; }))
        {
            allLayersValid = false;
            spdlog::error("Required validation layer {} not available", requiredLayer);
        }
    }

    return allLayersValid;
}

static vk::Bool32 debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                vk::DebugUtilsMessageTypeFlagsEXT,
                                const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
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

VulkanApplication::VulkanApplication() {}

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

    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();

        renderer_->renderFrame();
    }

    gpuDevice_->device().waitIdle();
}

void VulkanApplication::windowResized(int width, int height)
{
    renderer_->windowResized(width, height);
}

void VulkanApplication::initGlfw()
{
    glfwSetErrorCallback([](int errorCode, const char* description)
                         { spdlog::error("GLFW error {}: {}", errorCode, description); });

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwInitialised_ = true;
}

void VulkanApplication::initWindow(int windowWidth, int windowHeight,
                                   const std::string& windowTitle)
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
}

void VulkanApplication::initVulkan(int windowWidth, int windowHeight)
{
    const auto version = vk::enumerateInstanceVersion();
    spdlog::info("Vulkan API version: {}.{}.{}", VK_VERSION_MAJOR(version),
                 VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));

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
    gpuDevice_ = std::make_unique<GpuDevice>(instance_, surface_);

    spdlog::info("Creating renderer");
    renderer_ =
        std::make_unique<Renderer>(instance_, surface_, *gpuDevice_, windowWidth, windowHeight);
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

    constexpr auto appInfo = vk::ApplicationInfo{.pApplicationName = "Vulkan Demo",
                                                 .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                                 .pEngineName = "Vulkan Demo Engine",
                                                 .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                                 .apiVersion = vk::ApiVersion14};

    if (!validateExtensions(extensions, context_))
    {
        throw std::runtime_error("Requested extensions not available");
    }

    if (!validateLayers(validationLayers, context_))
    {
        throw std::runtime_error("Requested validation layers not available");
    }

    auto createInfo =
        vk::InstanceCreateInfo{.pApplicationInfo = &appInfo,
                               .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
                               .ppEnabledLayerNames = validationLayers.data(),
                               .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                               .ppEnabledExtensionNames = extensions.data()};

    instance_ = vk::raii::Instance(context_, createInfo);
}

void VulkanApplication::createDebugMessenger()
{
    const auto severityFlags = (vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    const auto messageTypeFlags = (vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    const auto debugUtilsMessengerCreateInfoEXT =
        vk::DebugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
                                             .messageType = messageTypeFlags,
                                             .pfnUserCallback = &debugCallback};

    debugMessenger_ = instance_.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
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