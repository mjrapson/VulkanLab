// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <ranges>
#include <stdexcept>
#include <vector>

constexpr bool validationLayersEnabled()
{
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

bool isDiscreteGpu(const vk::raii::PhysicalDevice& device)
{
    return device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
}

bool supportsGraphicsQueue(const vk::QueueFamilyProperties& properties)
{
    return (properties.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
}

uint32_t getGraphicsQueueFamilyIndex(const vk::raii::PhysicalDevice& device)
{
    const auto queueFamilyProperties = device.getQueueFamilyProperties();

    const auto itr = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
                                  supportsGraphicsQueue);

    if (itr == queueFamilyProperties.end())
    {
        throw std::runtime_error("Device does not support graphics queue family");
    }

    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), itr));
}

const auto validationLayers = std::vector<char const*>{"VK_LAYER_KHRONOS_validation"};
const auto deviceExtensions = std::vector<const char*>{
    vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName, vk::KHRCreateRenderpass2ExtensionName};

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
    initVulkan();
}

void VulkanApplication::run()
{
    spdlog::info("Running");

    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
    }
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
}

void VulkanApplication::initVulkan()
{
    auto version = uint32_t{0};
    auto pfnEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));

    if (pfnEnumerateInstanceVersion)
    {
        pfnEnumerateInstanceVersion(&version);
    }
    else
    {
        version = VK_API_VERSION_1_0;
    }

    const auto major = VK_VERSION_MAJOR(version);
    const auto minor = VK_VERSION_MINOR(version);
    const auto patch = VK_VERSION_PATCH(version);

    spdlog::info("Vulkan API version: {}.{}.{}", major, minor, patch);

    spdlog::info("Creating Vulkan instance");
    createVulkanInstance();

    spdlog::info("Creating window surface");
    createSurface();

    spdlog::info("Finding physical GPU device");
    pickPhysicalDevice();

    spdlog::info("Creating logical GPU device");
    createLogicalDevice();
}

void VulkanApplication::createVulkanInstance()
{
    constexpr auto appInfo = vk::ApplicationInfo{.pApplicationName = "Vulkan Demo",
                                                 .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                                 .pEngineName = "Vulkan Demo Engine",
                                                 .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                                 .apiVersion = vk::ApiVersion14};

    const auto requiredExtensions = getRequiredExtensions();
    const auto requiredLayers = getRequiredLayers();

    auto createInfo = vk::InstanceCreateInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()};

    instance_ = vk::raii::Instance(context_, createInfo);
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

void VulkanApplication::pickPhysicalDevice()
{
    const auto devices = instance_.enumeratePhysicalDevices();
    if (devices.empty())
    {
        throw std::runtime_error("Failed to find GPU with Vulkan support");
    }

    auto suitableDevices = std::vector<vk::raii::PhysicalDevice>{};
    for (const auto& device : devices)
    {
        if (isDeviceSuitable(device))
        {
            suitableDevices.push_back(device);
        }
    }

    if (suitableDevices.empty())
    {
        throw std::runtime_error("Failed to find a GPU with suitable Vulkan support");
    }

    physicalDevice_ = selecteBestDevice(suitableDevices);

    spdlog::info("Selected GPU: {}", std::string{physicalDevice_.getProperties().deviceName});
}

void VulkanApplication::createLogicalDevice()
{
    const auto graphicsQueueFamilyIndex = getGraphicsQueueFamilyIndex(physicalDevice_);

    auto queuePriority = 0.5f;
    const auto deviceQueueCreateInfo =
        vk::DeviceQueueCreateInfo{.queueFamilyIndex = graphicsQueueFamilyIndex,
                                  .queueCount = 1,
                                  .pQueuePriorities = &queuePriority};

    const auto featureChain =
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>{
            {}, {.dynamicRendering = true}, {.extendedDynamicState = true}};

    const auto deviceCreateInfo = vk::DeviceCreateInfo{
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()};

    device_ = vk::raii::Device(physicalDevice_, deviceCreateInfo);
    graphicsQueue_ = vk::raii::Queue(device_, graphicsQueueFamilyIndex, 0);
}

std::vector<char const*> VulkanApplication::getRequiredExtensions() const
{
    auto glfwExtensionCount = uint32_t{0};
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto extensions = std::vector<const char*>{glfwExtensions, glfwExtensions + glfwExtensionCount};
    if (validationLayersEnabled())
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    auto extensionProperties = context_.enumerateInstanceExtensionProperties();
    for (size_t i = 0; i < extensions.size(); ++i)
    {
        if (std::ranges::none_of(
                extensionProperties, [extension = extensions[i]](auto const& extensionProperty)
                { return strcmp(extensionProperty.extensionName, extension) == 0; }))
        {
            throw std::runtime_error("Required extension not supported: " +
                                     std::string(extensions[i]));
        }
    }

    return extensions;
}

std::vector<char const*> VulkanApplication::getRequiredLayers() const
{
    if (!validationLayersEnabled())
    {
        return std::vector<char const*>{};
    }

    const auto layerProperties = context_.enumerateInstanceLayerProperties();
    for (const auto& requiredLayer : validationLayers)
    {
        if (std::ranges::none_of(layerProperties, [requiredLayer](auto const& layerProperty)
                                 { return strcmp(layerProperty.layerName, requiredLayer) == 0; }))
        {
            throw std::runtime_error("One or more required layers are not supported");
        }
    }

    return validationLayers;
}

bool VulkanApplication::isDeviceSuitable(vk::raii::PhysicalDevice device) const
{
    const auto properties = device.getProperties();
    const auto deviceName = std::string{properties.deviceName};

    if (properties.apiVersion < VK_API_VERSION_1_3)
    {
        spdlog::info("Skipping {} - Vulkan API version too low ({})", deviceName,
                     properties.apiVersion);
        return false;
    }

    if (std::ranges::none_of(device.getQueueFamilyProperties(), supportsGraphicsQueue))
    {
        spdlog::info("Skipping {} - Does not support graphics queue family", deviceName);
        return false;
    }

    const auto extensionProperties = device.enumerateDeviceExtensionProperties();
    bool hasAllRequiredExtensions = true;
    for (const auto& requiredExtension : deviceExtensions)
    {
        if (std::ranges::none_of(
                extensionProperties, [&requiredExtension](auto const& extension)
                { return strcmp(extension.extensionName, requiredExtension) == 0; }))
        {
            hasAllRequiredExtensions = false;
            spdlog::info("Skipping {} - Does not support required device extension: {}", deviceName,
                         requiredExtension);
        }
    }

    if (!hasAllRequiredExtensions)
    {
        return false;
    }

    return true;
}

vk::raii::PhysicalDevice
VulkanApplication::selecteBestDevice(const std::vector<vk::raii::PhysicalDevice>& devices) const
{
    if (devices.empty())
    {
        throw std::invalid_argument("No devices to select between!");
    }

    if (devices.size() == 1)
    {
        return devices.at(0);
    }

    const auto itr = std::find_if(devices.begin(), devices.end(), isDiscreteGpu);
    if (itr != devices.end())
    {
        return *itr;
    }

    return devices.at(0);
}
