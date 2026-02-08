// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/gpu_device.h"

#include <core/file_system.h>

#include <spdlog/spdlog.h>

#include <ranges>

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

bool matchingMemoryIndex(uint32_t memoryIndex, uint32_t filter)
{
    return (filter & (1 << memoryIndex));
}

uint32_t findMemoryType(const vk::raii::PhysicalDevice& device, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{

    const auto memoryProperties = device.getMemoryProperties();
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if (!matchingMemoryIndex(i, typeFilter))
        {
            continue;
        }

        if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("No suitable memory type found");
}

vk::Extent2D getSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities, int windowWidth, int windowHeight)
{
    if (capabilities.currentExtent.width != 0xFFFFFFFF)
    {
        return capabilities.currentExtent;
    }

    return {std::clamp<uint32_t>(windowWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(windowHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

uint32_t getSurfaceMinImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities)
{
    const auto desiredImageCount = surfaceCapabilities.minImageCount + 1;

    if (surfaceCapabilities.maxImageCount == 0) // no maximum
    {
        return desiredImageCount;
    }

    return std::min(desiredImageCount, surfaceCapabilities.maxImageCount);
}

GpuDevice::GpuDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface)
{
    spdlog::info("Finding physical GPU device");
    pickPhysicalDevice(instance);

    spdlog::info("Creating logical GPU device");
    createLogicalDevice(surface);

    spdlog::info("Creating command pool");
    createCommandPool();
}

Swapchain GpuDevice::createSwapchain(const vk::raii::SurfaceKHR& surface, uint32_t width, uint32_t height) const
{
    auto swapchain = Swapchain{};

    const auto surfaceCapabilities = physicalDevice_.getSurfaceCapabilitiesKHR(*surface);
    swapchain.extent = getSwapchainExtent(surfaceCapabilities, width, height);
    swapchain.surfaceFormat = getSurfaceFormat(*surface);

    auto swapChainCreateInfo = vk::SwapchainCreateInfoKHR{};
    swapChainCreateInfo.surface = *surface;
    swapChainCreateInfo.minImageCount = getSurfaceMinImageCount(surfaceCapabilities);
    swapChainCreateInfo.imageFormat = swapchain.surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = swapchain.surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = swapchain.extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode = vk::PresentModeKHR::eFifo, swapChainCreateInfo.clipped = true;

    swapchain.swapchain = vk::raii::SwapchainKHR(device_, swapChainCreateInfo);
    swapchain.images = swapchain.swapchain.getImages();

    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = swapchain.surfaceFormat.format;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    for (const auto& image : swapchain.images)
    {
        imageViewCreateInfo.image = image;
        swapchain.views.emplace_back(device_, imageViewCreateInfo);
    }

    return swapchain;
}

vk::raii::CommandBuffers GpuDevice::createCommandBuffers(uint32_t count) const
{
    auto allocInfo = vk::CommandBufferAllocateInfo{};
    allocInfo.commandPool = *commandPool_;
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

vk::raii::DescriptorPool GpuDevice::createDescriptorPool(uint32_t maxSets,
                                                         std::span<vk::DescriptorPoolSize> poolSizes) const
{
    auto createInfo = vk::DescriptorPoolCreateInfo{};
    createInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    createInfo.maxSets = maxSets;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();

    return vk::raii::DescriptorPool{device_, createInfo};
}

vk::raii::DescriptorSetLayout
GpuDevice::createDescriptorSetLayout(const std::span<const vk::DescriptorSetLayoutBinding>& bindings) const
{
    auto createInfo = vk::DescriptorSetLayoutCreateInfo{};
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();

    return vk::raii::DescriptorSetLayout{device_, createInfo};
}

vk::raii::DescriptorSets GpuDevice::createDescriptorSets(const vk::raii::DescriptorSetLayout& layout,
                                                         const vk::raii::DescriptorPool& pool,
                                                         uint32_t count) const
{
    auto layouts = std::vector<vk::DescriptorSetLayout>{count, *layout};

    auto allocateInfo = vk::DescriptorSetAllocateInfo{};
    allocateInfo.descriptorPool = *pool;
    allocateInfo.descriptorSetCount = count;
    allocateInfo.pSetLayouts = layouts.data();

    return vk::raii::DescriptorSets{device_, allocateInfo};
}

vk::raii::ShaderModule GpuDevice::createShaderModule(const std::filesystem::path& filePath) const
{
    const auto code = core::readBinaryFile(filePath);

    auto createInfo = vk::ShaderModuleCreateInfo{};
    createInfo.codeSize = code.size() * sizeof(char);
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    return vk::raii::ShaderModule{device_, createInfo};
}

vk::raii::Buffer GpuDevice::createBuffer(const vk::DeviceSize& size,
                                         const vk::BufferUsageFlags& usage,
                                         const vk::SharingMode& sharingMode) const
{
    auto bufferInfo = vk::BufferCreateInfo{};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = sharingMode;

    return vk::raii::Buffer(device_, bufferInfo);
}

vk::raii::Buffer GpuDevice::createVertexBuffer(const vk::DeviceSize& size) const
{
    return createBuffer(size,
                        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                        vk::SharingMode::eExclusive);
}

vk::raii::Buffer GpuDevice::createIndexBuffer(const vk::DeviceSize& size) const
{
    return createBuffer(size,
                        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                        vk::SharingMode::eExclusive);
}

vk::raii::Buffer GpuDevice::createStagingBuffer(const vk::DeviceSize& size) const
{
    return createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive);
}

vk::raii::Buffer GpuDevice::createUniformBuffer(const vk::DeviceSize& size) const
{
    return createBuffer(size, vk::BufferUsageFlagBits::eUniformBuffer, vk::SharingMode::eExclusive);
}

void GpuDevice::copyBuffer(const vk::raii::CommandBuffer& cmd,
                           const vk::raii::Buffer& source,
                           const vk::raii::Buffer& destination,
                           const vk::DeviceSize& size,
                           const vk::DeviceSize& destinationOffset) const
{
    cmd.copyBuffer(*source, *destination, vk::BufferCopy(0, destinationOffset, size));
}

void GpuDevice::copyBufferToImage(const vk::raii::CommandBuffer& cmd,
                                  const vk::raii::Buffer& source,
                                  const vk::raii::Image& destination,
                                  uint32_t width,
                                  uint32_t height,
                                  uint32_t layers) const
{
    auto region = vk::BufferImageCopy{};
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = layers;
    region.imageExtent = vk::Extent3D{width, height, 1};

    cmd.copyBufferToImage(source, destination, vk::ImageLayout::eTransferDstOptimal, region);
}

vk::raii::Image GpuDevice::createImage(uint32_t width, uint32_t height) const
{
    auto imageInfo = vk::ImageCreateInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = vk::Format::eR8G8B8A8Srgb;
    imageInfo.extent = vk::Extent3D{width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    return vk::raii::Image{device_, imageInfo};
}

vk::raii::Image GpuDevice::createCubemapImage(uint32_t width, uint32_t height) const
{
    auto imageInfo = vk::ImageCreateInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = vk::Format::eR8G8B8A8Srgb;
    imageInfo.extent = vk::Extent3D{width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;

    return vk::raii::Image{device_, imageInfo};
}

vk::raii::Image GpuDevice::createDepthImage(uint32_t width, uint32_t height) const
{
    auto imageInfo = vk::ImageCreateInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = vk::Format::eD32Sfloat;
    imageInfo.extent = vk::Extent3D{width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    return vk::raii::Image{device_, imageInfo};
}

vk::raii::ImageView GpuDevice::createImageView(const vk::raii::Image& image) const
{
    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.image = *image;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = vk::Format::eR8G8B8A8Srgb;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    return vk::raii::ImageView{device_, imageViewCreateInfo};
}

vk::raii::ImageView GpuDevice::createDepthImageView(const vk::raii::Image& image) const
{
    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.image = *image;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = vk::Format::eD32Sfloat;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    return vk::raii::ImageView{device_, imageViewCreateInfo};
}

vk::raii::ImageView GpuDevice::createCubemapImageView(const vk::raii::Image& image) const
{
    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 6;

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.image = *image;
    imageViewCreateInfo.viewType = vk::ImageViewType::eCube;
    imageViewCreateInfo.format = vk::Format::eR8G8B8A8Srgb;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    return vk::raii::ImageView{device_, imageViewCreateInfo};
}

vk::raii::Sampler GpuDevice::createSampler() const
{
    auto samplerInfo = vk::SamplerCreateInfo{};
    samplerInfo.magFilter = vk::Filter::eNearest;
    samplerInfo.minFilter = vk::Filter::eNearest;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    return vk::raii::Sampler(device_, samplerInfo);
}

void GpuDevice::transitionImageLayout(const vk::Image& image,
                                      const vk::CommandBuffer& commandBuffer,
                                      vk::ImageLayout oldLayout,
                                      vk::ImageLayout newLayout,
                                      vk::AccessFlags2 srcAccessMask,
                                      vk::AccessFlags2 dstAccessMask,
                                      vk::PipelineStageFlags2 srcStageMask,
                                      vk::PipelineStageFlags2 dstStageMask,
                                      const vk::ImageAspectFlags& aspectFlags,
                                      uint32_t layerCount) const
{
    auto barrier = vk::ImageMemoryBarrier2{};
    barrier.srcStageMask = srcStageMask;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstStageMask = dstStageMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    auto dependencyInfo = vk::DependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    commandBuffer.pipelineBarrier2(dependencyInfo);
}

vk::raii::DeviceMemory GpuDevice::allocateBufferMemory(const vk::raii::Buffer& buffer,
                                                       vk::MemoryPropertyFlags properties) const
{
    const auto memoryRequirements = buffer.getMemoryRequirements();
    const auto memoryAllocateInfo = vk::MemoryAllocateInfo{
        memoryRequirements.size,
        findMemoryType(physicalDevice_, memoryRequirements.memoryTypeBits, properties)};

    auto memory = vk::raii::DeviceMemory(device_, memoryAllocateInfo);
    buffer.bindMemory(*memory, 0);

    return memory;
}

vk::raii::DeviceMemory GpuDevice::allocateDeviceBufferMemory(const vk::raii::Buffer& buffer) const
{
    return allocateBufferMemory(buffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
}

vk::raii::DeviceMemory GpuDevice::allocateStagingBufferMemory(const vk::raii::Buffer& buffer) const
{
    return allocateBufferMemory(buffer,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
}

vk::raii::DeviceMemory GpuDevice::allocateImageMemory(const vk::raii::Image& image,
                                                      vk::MemoryPropertyFlags properties) const
{
    const auto memRequirements = image.getMemoryRequirements();
    const auto memoryAllocateInfo = vk::MemoryAllocateInfo{
        memRequirements.size,
        findMemoryType(physicalDevice_, memRequirements.memoryTypeBits, properties)};

    auto memory = vk::raii::DeviceMemory{device_, memoryAllocateInfo};
    image.bindMemory(*memory, 0);

    return memory;
}

vk::SurfaceFormatKHR GpuDevice::getSurfaceFormat(const vk::SurfaceKHR& surface) const
{
    const auto formats = physicalDevice_.getSurfaceFormatsKHR(surface);

    if (formats.empty())
    {
        throw std::runtime_error("No available surface formats");
    }

    if (auto itr = std::ranges::find_if(formats,
                                        [](const vk::SurfaceFormatKHR& format)
                                        {
                                            return format.format == vk::Format::eB8G8R8A8Srgb
                                                   && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
                                        });
        itr != formats.end())
    {
        return *itr;
    }

    return formats.at(0);
}

vk::Result GpuDevice::present(const vk::PresentInfoKHR& info) const
{
    return presentQueue_.presentKHR(info);
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

bool GpuDevice::exceedsPushConstantLimit(size_t size) const
{
    return size > physicalDevice_.getProperties().limits.maxPushConstantsSize;
}

const vk::raii::Device& GpuDevice::device() const
{
    return device_;
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

void GpuDevice::createCommandPool()
{
    auto poolInfo = vk::CommandPoolCreateInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex_;

    commandPool_ = vk::raii::CommandPool(device_, poolInfo);
}

bool GpuDevice::isDeviceSuitable(vk::raii::PhysicalDevice device) const
{
    const auto properties = device.getProperties();
    const auto deviceName = std::string{properties.deviceName};

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
