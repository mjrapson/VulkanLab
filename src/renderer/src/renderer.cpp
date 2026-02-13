#include "renderer/renderer.h"

#include "render_passes/geometry_pass.h"
#include "render_passes/loading_screen_pass.h"
#include "render_passes/skybox_pass.h"
#include "renderer/camera.h"
#include "renderer/data.h"

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

void Renderer::setLoadingScreenImage(const ImageData& imageData)
{
    auto loadingScreenImage = std::unique_ptr<Image>();
    loadingScreenImage->image = gpuDevice_.createImage(imageData.width, imageData.height);
    loadingScreenImage->memory = gpuDevice_.allocateImageMemory(loadingScreenImage->image,
                                                                vk::MemoryPropertyFlagBits::eDeviceLocal);

    const auto imageSize = imageData.width * imageData.height * imageData.components;
    auto stagingBuffer = gpuDevice_.createStagingBuffer(imageSize);

    auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

    void* data = stagingMemory.mapMemory(0, imageSize);
    std::memcpy(data, imageData.data.data(), imageSize);
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

    gpuDevice_.copyBufferToImage(cmd, stagingBuffer, loadingScreenImage->image, imageData.width, imageData.height);

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

    loadingScreenPass_->rebuild(std::move(loadingScreenImage));
}

void Renderer::renderScene(std::span<const DrawCommand> drawCommands,
                           std::optional<renderer::SkyboxHandle> skyboxHandle,
                           const Camera& camera)
{
    renderFrame(
        [this, &drawCommands, &skyboxHandle, &camera](uint32_t imageIndex, const vk::raii::CommandBuffer& commandBuffer)
        {
            auto cameraBuffer = CameraBufferObject{};
            cameraBuffer.projection = camera.projection();
            cameraBuffer.view = camera.view();
            memcpy(cameraUboMappedMemory_[currentFrameIndex_], &cameraBuffer, sizeof(cameraBuffer));

            gpuDevice_.transitionImageLayout(swapchain_.images[imageIndex],
                                             commandBuffer,
                                             vk::ImageLayout::eUndefined,
                                             vk::ImageLayout::eColorAttachmentOptimal,
                                             {},
                                             vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                                             vk::ImageAspectFlagBits::eColor);

            skyboxPass_->recordCommands(currentFrameIndex_,
                                        commandBuffer,
                                        skyboxHandle.value(),
                                        swapchain_.views[imageIndex]);
            geometryPass_->recordCommands(currentFrameIndex_,
                                          commandBuffer,
                                          meshVertexBuffer_,
                                          meshIndexBuffer_,
                                          meshGpuData_,
                                          materialGpuData_,
                                          swapchain_.views[imageIndex],
                                          drawCommands);

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

void Renderer::setData(const AssetData& data)
{
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Empty image (move this)
    emptyImage_ = gpuDevice_.createImage(1, 1);
    emptyImageMemory_ = gpuDevice_.allocateImageMemory(emptyImage_, vk::MemoryPropertyFlagBits::eDeviceLocal);

    const auto imageSize = 4; //  RGBA8
    auto stagingBuffer = gpuDevice_.createBuffer(imageSize,
                                                 vk::BufferUsageFlagBits::eTransferSrc,
                                                 vk::SharingMode::eExclusive);

    auto stagingMemory = gpuDevice_.allocateBufferMemory(stagingBuffer,
                                                         vk::MemoryPropertyFlagBits::eHostVisible
                                                             | vk::MemoryPropertyFlagBits::eHostCoherent);

    auto imageData = std::vector<std::byte>{std::byte{1}, std::byte{1}, std::byte{1}, std::byte{1}};
    void* mappedMemory = stagingMemory.mapMemory(0, imageSize);
    std::memcpy(mappedMemory, imageData.data(), imageSize);
    stagingMemory.unmapMemory();

    gpuDevice_.transitionImageLayout(*emptyImage_,
                                     *cmd,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     {}, // srcAccess
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::PipelineStageFlagBits2::eTopOfPipe,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::ImageAspectFlagBits::eColor);

    gpuDevice_.copyBufferToImage(cmd, stagingBuffer, emptyImage_, 1, 1);

    gpuDevice_.transitionImageLayout(*emptyImage_,
                                     *cmd,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal,
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::AccessFlagBits2::eShaderRead,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::ImageAspectFlagBits::eColor);

    emptyImageView_ = gpuDevice_.createImageView(emptyImage_);
    emptyImageSampler_ = gpuDevice_.createSampler();

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    // Meshes
    uploadMeshes(data.meshData);

    // Images
    uploadImages(data.imageData);

    // Materials
    uploadMaterials(data.materialData);

    // Skyboxes
    uploadSkyboxes(data.skyboxData);

    skyboxPass_->rebuild(skyboxGpuData_);
    geometryPass_->rebuild(materialGpuData_);
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

void Renderer::uploadImages(const ImageDataContainer& data)
{

    for (const auto& [handle, imageData] : data)
    {
        auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
        auto& cmd = commandBuffers[0];
        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        const auto imageSize = imageData.width * imageData.height * imageData.components;

        auto image = Image{};
        image.image = gpuDevice_.createImage(imageData.width, imageData.height);
        image.memory = gpuDevice_.allocateImageMemory(image.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto stagingBuffer = gpuDevice_.createStagingBuffer(imageSize);
        auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

        void* mappedMemory = stagingMemory.mapMemory(0, imageSize);
        std::memcpy(mappedMemory, imageData.data.data(), imageSize);
        stagingMemory.unmapMemory();

        gpuDevice_.transitionImageLayout(*image.image,
                                         *cmd,
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         {}, // srcAccess
                                         vk::AccessFlagBits2::eTransferWrite,
                                         vk::PipelineStageFlagBits2::eTopOfPipe,
                                         vk::PipelineStageFlagBits2::eTransfer,
                                         vk::ImageAspectFlagBits::eColor);

        gpuDevice_.copyBufferToImage(cmd, stagingBuffer, image.image, imageData.width, imageData.height);

        gpuDevice_.transitionImageLayout(*image.image,
                                         *cmd,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal,
                                         vk::AccessFlagBits2::eTransferWrite,
                                         vk::AccessFlagBits2::eShaderRead,
                                         vk::PipelineStageFlagBits2::eTransfer,
                                         vk::PipelineStageFlagBits2::eFragmentShader,
                                         vk::ImageAspectFlagBits::eColor);

        image.view = gpuDevice_.createImageView(image.image);
        image.sampler = gpuDevice_.createSampler();

        imageGpuData_.emplace(handle, std::move(image));

        cmd.end();
        gpuDevice_.submitCommandBuffer(cmd);
    }
}

void Renderer::uploadMeshes(const MeshDataContainer& data)
{
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto totalVertices = size_t{0};
    auto totalIndices = size_t{0};

    for (const auto& [handle, mesh] : data)
    {
        totalVertices += mesh.vertices.size();
        totalIndices += mesh.indices.size();
    }

    const auto vertexBufferSize = static_cast<uint32_t>(sizeof(core::Vertex) * totalVertices);

    meshVertexBuffer_ = gpuDevice_.createVertexBuffer(vertexBufferSize);
    meshVertexBufferMemory_ = gpuDevice_.allocateDeviceBufferMemory(meshVertexBuffer_);

    auto vertexStagingBuffer = gpuDevice_.createStagingBuffer(vertexBufferSize);
    auto vertexStagingBufferMemory = gpuDevice_.allocateStagingBufferMemory(vertexStagingBuffer);

    const auto indexBufferSize = static_cast<uint32_t>(sizeof(uint32_t) * totalIndices);
    meshIndexBuffer_ = gpuDevice_.createIndexBuffer(indexBufferSize);
    meshIndexBufferMemory_ = gpuDevice_.allocateDeviceBufferMemory(meshIndexBuffer_);

    auto indexStagingBuffer = gpuDevice_.createStagingBuffer(indexBufferSize);
    auto indexStagingBufferMemory = gpuDevice_.allocateStagingBufferMemory(indexStagingBuffer);

    void* vertexStagingMemory = vertexStagingBufferMemory.mapMemory(0, vertexBufferSize);
    void* indexStagingMemory = indexStagingBufferMemory.mapMemory(0, indexBufferSize);

    auto currentVertexOffset = size_t{0};
    auto currentIndexOffset = size_t{0};
    for (const auto& [handle, meshData] : data)
    {
        auto mesh = Mesh{};
        mesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
        mesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
        mesh.vertexBufferOffset = static_cast<uint32_t>(currentVertexOffset);
        mesh.indexBufferOffset = static_cast<uint32_t>(currentIndexOffset);
        mesh.materialHandle = meshData.materialHandle;

        const auto vertexSize = meshData.vertices.size() * sizeof(core::Vertex);
        const auto indexSize = meshData.indices.size() * sizeof(uint32_t);

        std::memcpy(static_cast<std::byte*>(vertexStagingMemory) + currentVertexOffset * sizeof(core::Vertex),
                    meshData.vertices.data(),
                    vertexSize);

        std::memcpy(static_cast<std::byte*>(indexStagingMemory) + currentIndexOffset * sizeof(uint32_t),
                    meshData.indices.data(),
                    indexSize);

        currentVertexOffset += meshData.vertices.size();
        currentIndexOffset += meshData.indices.size();

        meshGpuData_.emplace(handle, std::move(mesh));
    }

    vertexStagingBufferMemory.unmapMemory();
    indexStagingBufferMemory.unmapMemory();

    gpuDevice_.copyBuffer(cmd, vertexStagingBuffer, meshVertexBuffer_, vertexBufferSize);
    gpuDevice_.copyBuffer(cmd, indexStagingBuffer, meshIndexBuffer_, indexBufferSize);

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);
}

void Renderer::uploadMaterials(const MaterialDataContainer& data)
{
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    const auto uboStride = gpuDevice_.calculateAlignedUboStride(sizeof(MaterialUboData));

    materialUbo_ = gpuDevice_.createUniformBuffer(uboStride * data.size());
    materialUboMemory_ = gpuDevice_.allocateStagingBufferMemory(materialUbo_);
    void* mapped = materialUboMemory_.mapMemory(0, VK_WHOLE_SIZE);

    auto offset = uint32_t{0};
    for (const auto& [handle, materialData] : data)
    {
        auto material = Material{};
        material.bufferOffset = offset;

        auto bufferData = MaterialUboData{};
        bufferData.diffuseColor = glm::vec4{materialData.diffuseColour, 1.0f};
        bufferData.hasDiffuseTexture = materialData.diffuseImage ? 1 : 0;

        std::memcpy(static_cast<std::byte*>(mapped) + offset, &bufferData, sizeof(MaterialUboData));

        offset += static_cast<uint32_t>(uboStride);

        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = *materialUbo_;
        bufferInfo.offset = 0;
        bufferInfo.range = uboStride;

        material.bufferInfo = bufferInfo;

        auto imageInfo = vk::DescriptorImageInfo{};
        if (materialData.diffuseImage)
        {
            imageInfo.imageView = *imageGpuData_.at(materialData.diffuseImage.value()).view;
            imageInfo.sampler = *imageGpuData_.at(materialData.diffuseImage.value()).sampler;
        }
        else
        {
            imageInfo.imageView = *emptyImageView_;
            imageInfo.sampler = *emptyImageSampler_;
        }
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        material.imageInfo = imageInfo;

        materialGpuData_.emplace(handle, std::move(material));
    }

    materialUboMemory_.unmapMemory();

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);
}

void Renderer::uploadSkyboxes(const SkyboxDataContainer& data)
{
    for (const auto& [handle, skyboxData] : data)
    {
        auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
        auto& cmd = commandBuffers[0];
        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        const auto width = skyboxData.imageData.at(0).width;
        const auto height = skyboxData.imageData.at(0).height;
        const auto components = skyboxData.imageData.at(0).components;
        const auto imageSize = width * height * components;

        auto skybox = Skybox{};
        skybox.image = gpuDevice_.createCubemapImage(width, height);
        skybox.memory = gpuDevice_.allocateImageMemory(skybox.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto stagingBuffer = gpuDevice_.createStagingBuffer(imageSize * 6);
        auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

        gpuDevice_.transitionImageLayout(*skybox.image,
                                         *cmd,
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         {}, // srcAccess
                                         vk::AccessFlagBits2::eTransferWrite,
                                         vk::PipelineStageFlagBits2::eTopOfPipe,
                                         vk::PipelineStageFlagBits2::eTransfer,
                                         vk::ImageAspectFlagBits::eColor,
                                         6);
        void* mappedMemory = stagingMemory.mapMemory(0, VK_WHOLE_SIZE);
        for (auto face = 0; face < 6; ++face)
        {
            std::memcpy(mappedMemory + (face * imageSize), skyboxData.imageData.at(face).data.data(), imageSize);
        }
        stagingMemory.unmapMemory();
        gpuDevice_.copyBufferToImage(cmd, stagingBuffer, skybox.image, width, height, 6);
        gpuDevice_.transitionImageLayout(*skybox.image,
                                         *cmd,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal,
                                         vk::AccessFlagBits2::eTransferWrite,
                                         vk::AccessFlagBits2::eShaderRead,
                                         vk::PipelineStageFlagBits2::eTransfer,
                                         vk::PipelineStageFlagBits2::eFragmentShader,
                                         vk::ImageAspectFlagBits::eColor,
                                         6);

        skybox.view = gpuDevice_.createCubemapImageView(skybox.image);
        skybox.sampler = gpuDevice_.createSampler();

        skyboxGpuData_.emplace(handle, std::move(skybox));

        cmd.end();
        gpuDevice_.submitCommandBuffer(cmd);
    }
}

} // namespace renderer
