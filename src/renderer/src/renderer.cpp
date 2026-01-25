#include "renderer/renderer.h"

#include "private/gpu_material.h"
#include "private/gpu_resource_cache.h"
#include "render_passes/geometry_pass.h"
#include "render_passes/skybox_pass.h"
#include "renderer/camera.h"
#include "renderer/gpu_device.h"
#include "renderer/vertex_layout.h"

#include <assets/asset_database.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <chrono>
#include <ranges>

namespace renderer
{
struct CameraBufferObject
{
    glm::mat4 view;
    glm::mat4 projection;
};

constexpr auto maxFramesInFlight = 2;

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

bool isPreferredSurfaceFormat(const vk::SurfaceFormatKHR& format)
{
    return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
}

vk::SurfaceFormatKHR getSurfaceFormat(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface)
{
    const auto formats = device.getSurfaceFormatsKHR(surface);

    if (formats.empty())
    {
        throw std::runtime_error("No available surface formats");
    }

    if (auto itr = std::ranges::find_if(formats, isPreferredSurfaceFormat); itr != formats.end())
    {
        return *itr;
    }

    return formats.at(0);
}

Renderer::Renderer(const vk::raii::Instance& instance,
                   const vk::raii::SurfaceKHR& surface,
                   const GpuDevice& gpuDevice,
                   int windowWidth,
                   int windowHeight)
    : instance_{instance},
      surface_{surface},
      gpuDevice_{gpuDevice},
      windowWidth_{windowWidth},
      windowHeight_{windowHeight}
{
    spdlog::info("Creating swapchain");
    createSwapchain();

    spdlog::info("Creating swapchain image views");
    createSwapchainImageViews();
    createDepthBufferImage();

    createCameraDescriptorPool();
    createDescriptorSetLayouts();
    createCameraBuffers();

    spdlog::info("Creating command buffers");
    createCommandBuffers();

    spdlog::info("Creating sync objects");
    createSyncObjects();

    spdlog::info("Creating render passes");
    createRenderPasses();
}

Renderer::~Renderer() = default;

void Renderer::renderFrame(const renderer::Camera& camera, const std::vector<DrawCommand>& drawCommands)
{
    if (gpuDevice_.device().waitForFences(*drawFences_.at(currentFrameIndex_), vk::True, UINT64_MAX)
        != vk::Result::eSuccess)
    {
        throw std::runtime_error("Device unable to wait for fence to signal");
    }

    auto result = vk::Result{};
    auto imageIndex = uint32_t{};

    try
    {
        std::tie(result, imageIndex) = swapchain_.acquireNextImage(UINT64_MAX,
                                                                   *presentCompleteSemaphores_.at(currentFrameIndex_),
                                                                   nullptr);
    }
    catch (const vk::OutOfDateKHRError&)
    {
        recreateSwapchain();
        return;
    }

    auto& commandBuffer = commandBuffers_.at(currentFrameIndex_);
    commandBuffer.reset();

    recordCommands(imageIndex, commandBuffer, camera, drawCommands);

    auto waitSemaphores = std::array{*presentCompleteSemaphores_.at(currentFrameIndex_)};
    auto signalSemaphores = std::array{*renderFinishedSemaphores_.at(imageIndex)};

    gpuDevice_.device().resetFences(*drawFences_.at(currentFrameIndex_));
    gpuDevice_.submitCommandBuffer(commandBuffer,
                                   waitSemaphores,
                                   vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput),
                                   signalSemaphores,
                                   *drawFences_.at(currentFrameIndex_));

    auto presentInfo = vk::PresentInfoKHR{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &*renderFinishedSemaphores_.at(imageIndex);
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &*swapchain_;
    presentInfo.pImageIndices = &imageIndex;

    try
    {
        result = gpuDevice_.presentQueue().presentKHR(presentInfo);
    }
    catch (const vk::OutOfDateKHRError&)
    {
        recreateSwapchain();
        return;
    }

    if (result == vk::Result::eSuboptimalKHR || windowResized_)
    {
        windowResized_ = false;
        recreateSwapchain();
    }

    currentFrameIndex_ = (currentFrameIndex_ + 1) & maxFramesInFlight;
}

void Renderer::windowResized(int width, int height)
{
    windowWidth_ = width;
    windowHeight_ = height;

    if (width == 0 && height == 0)
    {
        windowMinimized_ = true;
    }
    else
    {
        windowMinimized_ = false;
    }

    windowResized_ = true;
}

void Renderer::setResources(const assets::AssetDatabase& db)
{
    gpuResources_ = std::make_unique<GpuResourceCache>(db, gpuDevice_, maxFramesInFlight, materialDescriptorSetLayout_);
}

void Renderer::createSwapchain()
{
    const auto surfaceCapabilities = gpuDevice_.physicalDevice().getSurfaceCapabilitiesKHR(*surface_);
    swapchainExtent_ = getSwapchainExtent(surfaceCapabilities, windowWidth_, windowHeight_);
    surfaceFormat_ = getSurfaceFormat(*gpuDevice_.physicalDevice(), *surface_);

    auto swapChainCreateInfo = vk::SwapchainCreateInfoKHR{};
    swapChainCreateInfo.surface = *surface_;
    swapChainCreateInfo.minImageCount = getSurfaceMinImageCount(surfaceCapabilities);
    swapChainCreateInfo.imageFormat = surfaceFormat_.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat_.colorSpace;
    swapChainCreateInfo.imageExtent = swapchainExtent_, swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode = vk::PresentModeKHR::eFifo, swapChainCreateInfo.clipped = true;

    swapchain_ = vk::raii::SwapchainKHR(gpuDevice_.device(), swapChainCreateInfo);
    swapchainImages_ = swapchain_.getImages();
}

void Renderer::createSwapchainImageViews()
{
    swapchainImageViews_.clear();

    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = surfaceFormat_.format;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    for (const auto& image : swapchainImages_)
    {
        imageViewCreateInfo.image = image;
        swapchainImageViews_.emplace_back(gpuDevice_.device(), imageViewCreateInfo);
    }
}

void Renderer::createCameraDescriptorPool()
{
    auto cameraPoolSize = vk::DescriptorPoolSize{};
    cameraPoolSize.type = vk::DescriptorType::eUniformBuffer;
    cameraPoolSize.descriptorCount = maxFramesInFlight;

    auto cameraPoolSizes = std::array{cameraPoolSize};

    auto cameraPoolInfo = vk::DescriptorPoolCreateInfo{};
    cameraPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    cameraPoolInfo.maxSets = maxFramesInFlight;
    cameraPoolInfo.poolSizeCount = static_cast<uint32_t>(cameraPoolSizes.size());
    cameraPoolInfo.pPoolSizes = cameraPoolSizes.data();

    cameraDescriptorPool_ = vk::raii::DescriptorPool{gpuDevice_.device(), cameraPoolInfo};
}

void Renderer::createDescriptorSetLayouts()
{
    // Camera descriptor set layout
    auto cameraLayoutBinding = vk::DescriptorSetLayoutBinding{};
    cameraLayoutBinding.binding = 0;
    cameraLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    cameraLayoutBinding.descriptorCount = 1;
    cameraLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    auto cameraLayoutBindings = std::array{cameraLayoutBinding};

    auto cameraLayoutInfo = vk::DescriptorSetLayoutCreateInfo{};
    cameraLayoutInfo.bindingCount = static_cast<uint32_t>(cameraLayoutBindings.size());
    cameraLayoutInfo.pBindings = cameraLayoutBindings.data();

    cameraDescriptorSetLayout_ = vk::raii::DescriptorSetLayout(gpuDevice_.device(), cameraLayoutInfo);

    // Material descriptor set layout
    auto materialUboLayoutBinding = vk::DescriptorSetLayoutBinding{};
    materialUboLayoutBinding.binding = 0;
    materialUboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
    materialUboLayoutBinding.descriptorCount = 1;
    materialUboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    auto textureBinding = vk::DescriptorSetLayoutBinding{};
    textureBinding.binding = 1;
    textureBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    auto materialLayoutBindings = std::array{materialUboLayoutBinding, textureBinding};

    auto materialLayoutInfo = vk::DescriptorSetLayoutCreateInfo{};
    materialLayoutInfo.bindingCount = static_cast<uint32_t>(materialLayoutBindings.size());
    materialLayoutInfo.pBindings = materialLayoutBindings.data();

    materialDescriptorSetLayout_ = vk::raii::DescriptorSetLayout(gpuDevice_.device(), materialLayoutInfo);
}

void Renderer::createCommandBuffers()
{
    commandBuffers_ = gpuDevice_.createCommandBuffers(maxFramesInFlight);
}

void Renderer::createSyncObjects()
{
    for ([[maybe_unused]] auto _ : std::views::repeat(0, swapchainImages_.size()))
    {
        renderFinishedSemaphores_.emplace_back(gpuDevice_.device(), vk::SemaphoreCreateInfo{});
    }

    for ([[maybe_unused]] auto _ : std::views::repeat(0, maxFramesInFlight))
    {
        presentCompleteSemaphores_.emplace_back(gpuDevice_.device(), vk::SemaphoreCreateInfo{});

        auto fenceCreateInfo = vk::FenceCreateInfo{};
        fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;
        drawFences_.emplace_back(gpuDevice_.device(), fenceCreateInfo);
    }
}

void Renderer::createCameraBuffers()
{
    auto layouts = std::vector<vk::DescriptorSetLayout>{maxFramesInFlight, *cameraDescriptorSetLayout_};

    auto allocInfo = vk::DescriptorSetAllocateInfo{};
    allocInfo.descriptorPool = *cameraDescriptorPool_;
    allocInfo.descriptorSetCount = maxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    cameraDescriptorSets_ = std::move(vk::raii::DescriptorSets{gpuDevice_.device(), allocInfo});

    for (auto frameIndex = 0; frameIndex < maxFramesInFlight; ++frameIndex)
    {
        auto buffer = gpuDevice_.createBuffer(sizeof(CameraBufferObject),
                                              vk::BufferUsageFlagBits::eUniformBuffer,
                                              vk::SharingMode::eExclusive);

        auto memory = gpuDevice_.allocateBufferMemory(buffer,
                                                      vk::MemoryPropertyFlagBits::eHostVisible
                                                          | vk::MemoryPropertyFlagBits::eHostCoherent);

        auto mappedMemory = memory.mapMemory(0, VK_WHOLE_SIZE);

        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraBufferObject);

        auto uboWrite = vk::WriteDescriptorSet{};
        uboWrite.dstSet = cameraDescriptorSets_.at(frameIndex);
        uboWrite.dstBinding = 0;
        uboWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        auto writes = std::array{uboWrite};
        gpuDevice_.device().updateDescriptorSets(writes, {});

        cameraUboBuffers_.emplace_back(std::move(buffer));
        cameraUboBuffersMemory_.emplace_back(std::move(memory));
        cameraUboMappedMemory_.emplace_back(std::move(mappedMemory));
    }
}

void Renderer::recreateSwapchain()
{
    if (windowMinimized_)
    {
        return;
    }

    gpuDevice_.device().waitIdle();

    swapchainImageViews_.clear();
    swapchain_ = nullptr;

    createSwapchain();
    createSwapchainImageViews();
    createDepthBufferImage();
}

void Renderer::recordCommands(uint32_t imageIndex,
                              const vk::raii::CommandBuffer& commandBuffer,
                              const renderer::Camera& camera,
                              const std::vector<DrawCommand>& drawCommands)
{
    commandBuffer.begin({});

    auto cameraBuffer = CameraBufferObject{};
    cameraBuffer.projection = camera.projection();
    cameraBuffer.view = camera.view();

    memcpy(cameraUboMappedMemory_[currentFrameIndex_], &cameraBuffer, sizeof(cameraBuffer));

    auto passInfo = RenderPassCommandInfo{
        .frameIndex = currentFrameIndex_,
        .colorImage = swapchainImages_[imageIndex],
        .colorImageView = swapchainImageViews_[imageIndex],
        .depthImage = depthImage_,
        .depthImageView = depthImageView_,
        .extent = swapchainExtent_,
        .commandBuffer = commandBuffer,
        .cameraDescriptorSet = cameraDescriptorSets_.at(currentFrameIndex_),
        .gpuResourceCache = *gpuResources_,
        .drawCommands = drawCommands,
    };

    gpuDevice_.transitionImageLayout(swapchainImages_[imageIndex],
                                     commandBuffer,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eColorAttachmentOptimal,
                                     {},
                                     vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                                     vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                     vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                                     vk::ImageAspectFlagBits::eColor);

    skyboxPass_->recordCommands(passInfo);
    geometryPass_->recordCommands(passInfo);

    gpuDevice_.transitionImageLayout(swapchainImages_[imageIndex],
                                     commandBuffer,
                                     vk::ImageLayout::eColorAttachmentOptimal,
                                     vk::ImageLayout::ePresentSrcKHR,
                                     vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                                     {},                                                 // dstAccessMask
                                     vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                     vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
                                     vk::ImageAspectFlagBits::eColor);

    commandBuffer.end();
}

void Renderer::createDepthBufferImage()
{
    depthImage_ = gpuDevice_.createDepthImage(swapchainExtent_.width, swapchainExtent_.height);
    depthImageMemory_ = gpuDevice_.allocateImageMemory(depthImage_, vk::MemoryPropertyFlagBits::eDeviceLocal);
    depthImageView_ = gpuDevice_.createDepthImageView(depthImage_);
}

void Renderer::createRenderPasses()
{
    skyboxPass_ = std::make_unique<SkyboxPass>(gpuDevice_, surfaceFormat_.format, cameraDescriptorSetLayout_);

    geometryPass_ = std::make_unique<GeometryPass>(gpuDevice_,
                                                   surfaceFormat_.format,
                                                   cameraDescriptorSetLayout_,
                                                   materialDescriptorSetLayout_);
}
} // namespace renderer
