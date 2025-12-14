// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/gpu_device.h"

#include <spdlog/spdlog.h>

#include <ranges>

const auto deviceExtensions = std::vector<const char*>{
    vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName, vk::KHRCreateRenderpass2ExtensionName};

bool isDiscreteGpu(const vk::raii::PhysicalDevice& device)
{
    return device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
}

bool supportsGraphicsQueue(const vk::QueueFamilyProperties& properties)
{
    return (properties.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
}

bool supportsSurfacePresentation(uint32_t index, const vk::raii::PhysicalDevice& device,
                                 const vk::raii::SurfaceKHR& surface)
{
    return (device.getSurfaceSupportKHR(index, surface) == VK_TRUE);
}

uint32_t getGraphicsQueueFamilyIndex(const vk::raii::PhysicalDevice& device)
{
    const auto& queueFamilyProperties = device.getQueueFamilyProperties();

    const auto itr = std::ranges::find_if(queueFamilyProperties, supportsGraphicsQueue);

    if (itr == queueFamilyProperties.end())
    {
        throw std::runtime_error("Device does not support graphics queue family");
    }

    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), itr));
}

uint32_t getSurfacePresentationQueueFamilyIndex(const vk::raii::PhysicalDevice& device,
                                                const vk::raii::SurfaceKHR& surface)
{
    const auto& queueFamilyProperties = device.getQueueFamilyProperties();

    const auto itr = std::ranges::find_if(queueFamilyProperties,
                                          [&device, &surface, idx = size_t{0}](const auto&) mutable
                                          {
                                              const auto validQueueFamilyProperty =
                                                  supportsSurfacePresentation(idx, device, surface);

                                              ++idx;
                                              return validQueueFamilyProperty;
                                          });

    if (itr == queueFamilyProperties.end())
    {
        throw std::runtime_error("Device does not support surface presentation queue family");
    }

    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), itr));
}

GpuDevice::GpuDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface)
{
    spdlog::info("Finding physical GPU device");
    pickPhysicalDevice(instance);

    spdlog::info("Creating logical GPU device");
    createLogicalDevice(surface);
}

const vk::raii::Device& GpuDevice::device() const
{
    return device_;
}

const vk::raii::PhysicalDevice& GpuDevice::physicalDevice() const
{
    return physicalDevice_;
}

const vk::raii::Queue& GpuDevice::graphicsQueue() const
{
    return graphicsQueue_;
}

const vk::raii::Queue& GpuDevice::presentQueue() const
{
    return presentQueue_;
}

uint32_t GpuDevice::graphicsQueueFamilyIndex() const
{
    return graphicsQueueFamilyIndex_;
}

void GpuDevice::pickPhysicalDevice(const vk::raii::Instance& instance)
{
    const auto devices = instance.enumeratePhysicalDevices();
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

    physicalDevice_ = selectBestDevice(suitableDevices);

    spdlog::info("Selected GPU: {}", std::string{physicalDevice_.getProperties().deviceName});
}

void GpuDevice::createLogicalDevice(const vk::raii::SurfaceKHR& surface)
{
    graphicsQueueFamilyIndex_ = getGraphicsQueueFamilyIndex(physicalDevice_);

    const auto surfacePresentationQueueFamilyIndex =
        getSurfacePresentationQueueFamilyIndex(physicalDevice_, surface);

    auto queuePriority = 0.5f;
    const auto deviceQueueCreateInfo =
        vk::DeviceQueueCreateInfo{.queueFamilyIndex = graphicsQueueFamilyIndex_,
                                  .queueCount = 1,
                                  .pQueuePriorities = &queuePriority};

    const auto featureChain =
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>{
            {},
            {.shaderDrawParameters = true},
            {.synchronization2 = true, .dynamicRendering = true},
            {.extendedDynamicState = true}};

    const auto deviceCreateInfo = vk::DeviceCreateInfo{
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()};

    device_ = vk::raii::Device(physicalDevice_, deviceCreateInfo);
    graphicsQueue_ = vk::raii::Queue(device_, graphicsQueueFamilyIndex_, 0);
    presentQueue_ = vk::raii::Queue(device_, surfacePresentationQueueFamilyIndex, 0);
}

bool GpuDevice::isDeviceSuitable(vk::raii::PhysicalDevice device) const
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
GpuDevice::selectBestDevice(const std::vector<vk::raii::PhysicalDevice>& devices) const
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
