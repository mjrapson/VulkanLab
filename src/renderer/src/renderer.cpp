#include "renderer/renderer.h"

#include "render_passes/geometry_pass.h"
#include "render_passes/loading_screen_pass.h"
#include "render_passes/skybox_pass.h"
#include "renderer/camera.h"
#include "renderer/gpu_resource_cache.h"

#include <assets/image.h>
#include <window/window.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <ranges>

namespace renderer
{
struct CameraBufferObject
{
    glm::mat4 view;
    glm::mat4 projection;
};

constexpr auto maxFramesInFlight = 2;

Renderer::Renderer(const window::Window& window)
    : instance_{context_, window.requiredExtensions()},
      surface_{window.createVulkanSurface(instance_.instance())},
      gpuDevice_{instance_.instance(), surface_},
      windowWidth_{window.width()},
      windowHeight_{window.height()},
      commandPool_{gpuDevice_.createCommandPool()}
{
    spdlog::info("Creating swapchain");
    createSwapchain();

    spdlog::info("Creating command buffers");
    createCommandBuffers();

    spdlog::info("Creating sync objects");
    createSyncObjects();

    spdlog::info("Creating render passes");
    createCameraBuffers();
    createRenderPasses();
}

Renderer::~Renderer() = default;

void Renderer::setLoadingScreenImage(const assets::Image& image)
{
    auto loadingScreenImage = std::make_unique<GpuImage>();
    loadingScreenImage->image = gpuDevice_.createImage(image.width(), image.height());
    loadingScreenImage->memory = gpuDevice_.allocateImageMemory(loadingScreenImage->image,
                                                                vk::MemoryPropertyFlagBits::eDeviceLocal);

    const auto imageSize = image.width() * image.height() * 4;
    auto stagingBuffer = gpuDevice_.createStagingBuffer(imageSize);

    auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

    void* data = stagingMemory.mapMemory(0, imageSize);
    std::memcpy(data, image.data().data(), imageSize);
    stagingMemory.unmapMemory();

    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    gpuDevice_.transitionImageLayout(*loadingScreenImage->image,
                                     *cmd,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     {}, // srcAccess
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::PipelineStageFlagBits2::eTopOfPipe,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::ImageAspectFlagBits::eColor);

    gpuDevice_.copyBufferToImage(cmd, stagingBuffer, loadingScreenImage->image, image.width(), image.height());

    gpuDevice_.transitionImageLayout(*loadingScreenImage->image,
                                     *cmd,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal,
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::AccessFlagBits2::eShaderRead,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::ImageAspectFlagBits::eColor);

    loadingScreenImage->view = gpuDevice_.createImageView(loadingScreenImage->image);
    loadingScreenImage->sampler = gpuDevice_.createSampler();

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    loadingScreenPass_->setImage(std::move(loadingScreenImage));
}

void Renderer::renderScene(std::span<const DrawCommand> drawCommands,
                           std::optional<assets::SkyboxHandle> skyboxHandle,
                           const Camera& camera)
{
    renderFrame(
        [this, &drawCommands, &skyboxHandle, &camera](uint32_t imageIndex, const vk::raii::CommandBuffer& commandBuffer)
        {
            auto cameraBuffer = CameraBufferObject{};
            cameraBuffer.projection = camera.projection();
            cameraBuffer.view = camera.view();
            memcpy(cameraUboMappedMemory_[currentFrameIndex_], &cameraBuffer, sizeof(cameraBuffer));

            auto passInfo = RenderPassCommandInfo{
                .frameIndex = currentFrameIndex_,
                .commandBuffer = commandBuffer,
                .skyboxHandle = skyboxHandle,
                .gpuResourceCache = *gpuResources_,
                .drawCommands = drawCommands,
            };

            gpuDevice_.transitionImageLayout(swapchain_.images[imageIndex],
                                             commandBuffer,
                                             vk::ImageLayout::eUndefined,
                                             vk::ImageLayout::eColorAttachmentOptimal,
                                             {},
                                             vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                                             vk::ImageAspectFlagBits::eColor);

            skyboxPass_->recordCommands(passInfo, swapchain_.views[imageIndex]);
            geometryPass_->recordCommands(passInfo, swapchain_.views[imageIndex]);

            gpuDevice_.transitionImageLayout(swapchain_.images[imageIndex],
                                             commandBuffer,
                                             vk::ImageLayout::eColorAttachmentOptimal,
                                             vk::ImageLayout::ePresentSrcKHR,
                                             vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                                             {},                                                 // dstAccessMask
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                             vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
                                             vk::ImageAspectFlagBits::eColor);
        });
}

void Renderer::renderLoadingScreen()
{
    renderFrame(
        [this](uint32_t imageIndex, const vk::raii::CommandBuffer& commandBuffer)
        {
            gpuDevice_.transitionImageLayout(swapchain_.images[imageIndex],
                                             commandBuffer,
                                             vk::ImageLayout::eUndefined,
                                             vk::ImageLayout::eColorAttachmentOptimal,
                                             {},
                                             vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                                             vk::ImageAspectFlagBits::eColor);

            loadingScreenPass_->recordCommands(currentFrameIndex_, commandBuffer, swapchain_.views[imageIndex]);

            gpuDevice_.transitionImageLayout(swapchain_.images[imageIndex],
                                             commandBuffer,
                                             vk::ImageLayout::eColorAttachmentOptimal,
                                             vk::ImageLayout::ePresentSrcKHR,
                                             vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                                             {},                                                 // dstAccessMask
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                             vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
                                             vk::ImageAspectFlagBits::eColor);
        });
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

void Renderer::setResources(std::unique_ptr<GpuResourceCache> gpuResources)
{
    gpuResources_ = std::move(gpuResources);
    gpuResources_->submitPendingCommands();

    skyboxPass_->rebuild(*gpuResources_);
    geometryPass_->rebuild(*gpuResources_);
}

const GpuDevice& Renderer::device() const
{
    return gpuDevice_;
}

void Renderer::createSwapchain()
{
    swapchain_ = gpuDevice_.createSwapchain(surface_,
                                            static_cast<uint32_t>(windowWidth_),
                                            static_cast<uint32_t>(windowHeight_));
}

void Renderer::createCommandBuffers()
{
    commandBuffers_ = gpuDevice_.createCommandBuffers(commandPool_, maxFramesInFlight);
}

void Renderer::createSyncObjects()
{
    for ([[maybe_unused]] auto _ : std::views::repeat(0, swapchain_.images.size()))
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
    for (auto frameIndex = 0; frameIndex < maxFramesInFlight; ++frameIndex)
    {
        auto buffer = gpuDevice_.createUniformBuffer(sizeof(CameraBufferObject));
        auto memory = gpuDevice_.allocateStagingBufferMemory(buffer);

        auto mappedMemory = memory.mapMemory(0, VK_WHOLE_SIZE);

        cameraUboBuffers_.push_back(std::move(buffer));
        cameraUboBuffersMemory_.push_back(std::move(memory));
        cameraUboMappedMemory_.push_back(std::move(mappedMemory));
    }
}

void Renderer::createRenderPasses()
{
    loadingScreenPass_ = std::make_unique<LoadingScreenPass>(gpuDevice_);
    loadingScreenPass_->initialize(swapchain_.extent, swapchain_.surfaceFormat.format, maxFramesInFlight);

    skyboxPass_ = std::make_unique<SkyboxPass>(gpuDevice_);
    skyboxPass_->initialize(swapchain_.extent, swapchain_.surfaceFormat.format, maxFramesInFlight, cameraUboBuffers_);

    geometryPass_ = std::make_unique<GeometryPass>(gpuDevice_);
    geometryPass_->initialize(swapchain_.extent, swapchain_.surfaceFormat.format, maxFramesInFlight, cameraUboBuffers_);
}

void Renderer::recreateSwapchain()
{
    if (windowMinimized_)
    {
        return;
    }

    gpuDevice_.device().waitIdle();

    swapchain_ = Swapchain{};
    createSwapchain();

    loadingScreenPass_->resize(swapchain_.extent);
    skyboxPass_->resize(swapchain_.extent);
    geometryPass_->resize(swapchain_.extent);
}

void Renderer::renderFrame(std::function<void(uint32_t, const vk::raii::CommandBuffer&)> recordCommands)
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
        std::tie(result,
                 imageIndex) = swapchain_.swapchain.acquireNextImage(UINT64_MAX,
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
    commandBuffer.begin({});

    recordCommands(imageIndex, commandBuffer);

    commandBuffer.end();

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
    presentInfo.pSwapchains = &*swapchain_.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    try
    {
        result = gpuDevice_.present(presentInfo);
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

    gpuDevice_.device().waitIdle();
}
} // namespace renderer
