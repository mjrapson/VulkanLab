// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/gpu_device.h"

#include <core/file_system.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <ranges>
#include <stdexcept>

namespace renderer
{
const auto deviceExtensions = std::vector<const char*>{vk::KHRSwapchainExtensionName};

bool isDiscreteGpu(const vk::raii::PhysicalDevice& device)
{
    return device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
}

bool supportsGraphicsQueue(const vk::QueueFamilyProperties& properties)
{
    return (properties.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
}

bool supportsSurfacePresentation(uint32_t index,
                                 const vk::raii::PhysicalDevice& device,
                                 const vk::raii::SurfaceKHR& surface)
{
    return (device.getSurfaceSupportKHR(index, *surface) == VK_TRUE);
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

    const auto itr = std::ranges::find_if(
        queueFamilyProperties,
        [&device, &surface, idx = uint32_t{0}](const auto&) mutable
        {
            const auto validQueueFamilyProperty = supportsSurfacePresentation(idx, device, surface);

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

    spdlog::info("Creating resource allocator");
    auto allocatorInfo = VmaAllocatorCreateInfo{};
    allocatorInfo.physicalDevice = *physicalDevice_;
    allocatorInfo.device = *device_;
    allocatorInfo.instance = *instance;
    allocatorInfo.vulkanApiVersion = physicalDevice_.getProperties().apiVersion;

    if (vmaCreateAllocator(&allocatorInfo, &allocator_) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

GpuDevice::~GpuDevice()
{
    vmaDestroyAllocator(allocator_);
}

vk::raii::CommandPool GpuDevice::createCommandPool() const
{
    auto poolInfo = vk::CommandPoolCreateInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex_;

    return vk::raii::CommandPool(device_, poolInfo);
}

vk::raii::CommandBuffers GpuDevice::createCommandBuffers(const vk::raii::CommandPool& pool, uint32_t count) const
{
    auto allocInfo = vk::CommandBufferAllocateInfo{};
    allocInfo.commandPool = *pool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = count;

    return vk::raii::CommandBuffers(device_, allocInfo);
}

void GpuDevice::submitCommandBuffer(const vk::CommandBuffer& cmd) const
{
    auto submitInfo = vk::SubmitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    graphicsQueue_.submit(submitInfo);
    graphicsQueue_.waitIdle();
}

void GpuDevice::submitCommandBuffer(const vk::CommandBuffer& cmd,
                                    std::span<vk::Semaphore> waitSemaphores,
                                    const vk::PipelineStageFlags& waitStageMask,
                                    std::span<vk::Semaphore> signalSemaphores,
                                    const vk::Fence& fence) const
{
    auto submitInfo = vk::SubmitInfo{};
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    graphicsQueue_.submit(submitInfo, fence);
}

vk::raii::ShaderModule GpuDevice::createShaderModule(const std::filesystem::path& filePath) const
{
    const auto code = core::readBinaryFile(filePath);

    auto createInfo = vk::ShaderModuleCreateInfo{};
    createInfo.codeSize = code.size() * sizeof(char);
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    return vk::raii::ShaderModule{device_, createInfo};
}

vk::DeviceSize GpuDevice::calculateAlignedUboStride(size_t uboSize) const
{
    const auto alignment = physicalDevice_.getProperties().limits.minUniformBufferOffsetAlignment;

    if (uboSize < alignment || uboSize == alignment)
    {
        return alignment;
    }

    return uboSize + (alignment - (uboSize % alignment));
}

vk::raii::Device& GpuDevice::device()
{
    return device_;
}

const vk::raii::PhysicalDevice& GpuDevice::physicalDevice() const
{
    return physicalDevice_;
}

const vk::raii::Queue& GpuDevice::presentQueue() const
{
    return presentQueue_;
}

VmaAllocator GpuDevice::allocator() const
{
    return allocator_;
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

    spdlog::info("Selected GPU: {}", std::string{physicalDevice_.getProperties().deviceName.data()});
}

void GpuDevice::createLogicalDevice(const vk::raii::SurfaceKHR& surface)
{
    graphicsQueueFamilyIndex_ = getGraphicsQueueFamilyIndex(physicalDevice_);

    const auto surfacePresentationQueueFamilyIndex = getSurfacePresentationQueueFamilyIndex(physicalDevice_, surface);

    auto queuePriority = 0.5f;
    auto deviceQueueCreateInfo = vk::DeviceQueueCreateInfo{};
    deviceQueueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex_;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    auto deviceFeatures = vk::PhysicalDeviceFeatures2{};

    auto vulkan11Features = vk::PhysicalDeviceVulkan11Features{};
    vulkan11Features.shaderDrawParameters = true;

    auto vulkan13Features = vk::PhysicalDeviceVulkan13Features{};
    vulkan13Features.synchronization2 = true;
    vulkan13Features.dynamicRendering = true;

    auto extendedDynamicStateFeatures = vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{};
    extendedDynamicStateFeatures.extendedDynamicState = true;

    auto featureChain = vk::StructureChain<vk::PhysicalDeviceFeatures2,
                                           vk::PhysicalDeviceVulkan11Features,
                                           vk::PhysicalDeviceVulkan13Features,
                                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>{
        deviceFeatures,
        vulkan11Features,
        vulkan13Features,
        extendedDynamicStateFeatures};

    auto deviceCreateInfo = vk::DeviceCreateInfo{};
    deviceCreateInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    device_ = vk::raii::Device(physicalDevice_, deviceCreateInfo);
    graphicsQueue_ = vk::raii::Queue(device_, graphicsQueueFamilyIndex_, 0);
    presentQueue_ = vk::raii::Queue(device_, surfacePresentationQueueFamilyIndex, 0);
}

bool GpuDevice::isDeviceSuitable(vk::raii::PhysicalDevice device) const
{
    const auto properties = device.getProperties();
    const auto deviceName = std::string{properties.deviceName.data()};

    if (properties.apiVersion < VK_API_VERSION_1_3)
    {
        spdlog::info("Skipping {} - Vulkan API version too low ({})", deviceName, properties.apiVersion);
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
        if (std::ranges::none_of(extensionProperties,
                                 [&requiredExtension](auto const& extension)
                                 {
                                     return strcmp(extension.extensionName, requiredExtension) == 0;
                                 }))
        {
            hasAllRequiredExtensions = false;
            spdlog::info("Skipping {} - Does not support required device extension: {}", deviceName, requiredExtension);
        }
    }

    if (!hasAllRequiredExtensions)
    {
        return false;
    }

    return true;
}

vk::raii::PhysicalDevice GpuDevice::selectBestDevice(const std::vector<vk::raii::PhysicalDevice>& devices) const
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
} // namespace renderer
