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

    createCameraBuffers();

    spdlog::info("Creating command buffers");
    createCommandBuffers();

    spdlog::info("Creating sync objects");
    createSyncObjects();

    spdlog::info("Creating render passes");
    createRenderPasses();
}

Renderer::~Renderer() = default;

void Renderer::queueMeshDraw(assets::SubMeshHandle subMeshHandle,
                             assets::MaterialHandle materialHandle,
                             const glm::mat4& transform)
{
    auto drawCommand = DrawCommand{};
    drawCommand.subMeshHandle = subMeshHandle;
    drawCommand.materialHandle = materialHandle;
    drawCommand.transform = transform;

    drawCommands_.push_back(drawCommand);
}

void Renderer::renderFrame(const renderer::Camera& camera, const std::optional<assets::SkyboxHandle>& skyboxHandle)
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

    recordCommands(imageIndex, commandBuffer, camera, skyboxHandle);
    drawCommands_.clear();

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
    gpuResources_ = std::make_unique<GpuResourceCache>(db, gpuDevice_);

    skyboxPass_->rebuild(*gpuResources_);
    geometryPass_->rebuild(*gpuResources_);
}

void Renderer::createSwapchain()
{
    swapchain_ = gpuDevice_.createSwapchain(surface_, windowWidth_, windowHeight_);
}

void Renderer::createCommandBuffers()
{
    commandBuffers_ = gpuDevice_.createCommandBuffers(maxFramesInFlight);
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

    swapchain_ = Swapchain{};
    createSwapchain();

    skyboxPass_->resize(swapchain_.extent);
    geometryPass_->resize(swapchain_.extent);
}

void Renderer::recordCommands(uint32_t imageIndex,
                              const vk::raii::CommandBuffer& commandBuffer,
                              const renderer::Camera& camera,
                              const std::optional<assets::SkyboxHandle>& skyboxHandle)
{
    commandBuffer.begin({});

    auto cameraBuffer = CameraBufferObject{};
    cameraBuffer.projection = camera.projection();
    cameraBuffer.view = camera.view();

    memcpy(cameraUboMappedMemory_[currentFrameIndex_], &cameraBuffer, sizeof(cameraBuffer));

    auto passInfo = RenderPassCommandInfo{
        .frameIndex = currentFrameIndex_,
        .commandBuffer = commandBuffer,
        .skyboxHandle = skyboxHandle,
        .gpuResourceCache = *gpuResources_,
        .drawCommands = drawCommands_,
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

    commandBuffer.end();
}

void Renderer::createRenderPasses()
{
    skyboxPass_ = std::make_unique<SkyboxPass>(gpuDevice_);
    skyboxPass_->initialize(swapchain_.extent, swapchain_.surfaceFormat.format, maxFramesInFlight, cameraUboBuffers_);

    geometryPass_ = std::make_unique<GeometryPass>(gpuDevice_);
    geometryPass_->initialize(swapchain_.extent, swapchain_.surfaceFormat.format, maxFramesInFlight, cameraUboBuffers_);
}
} // namespace renderer
