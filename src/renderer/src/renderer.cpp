#include "renderer/renderer.h"

#include "render_passes/geometry_pass.h"
#include "render_passes/loading_screen_pass.h"
#include "render_passes/skybox_pass.h"
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
    createDirectionalLightBuffers();
    createRenderPasses();
}

Renderer::~Renderer() = default;

void Renderer::setLoadingScreenImageData(const ImageDataContainer& imageData)
{
    loadingScreenGpuData_ = uploadImages(imageData);

    loadingScreenPass_->regenerateDescriptorSets(loadingScreenGpuData_);
}

void Renderer::renderScene(const SceneDrawInfo& info)
{
    renderFrame(
        [this, &info](uint32_t imageIndex, const vk::raii::CommandBuffer& commandBuffer)
        {
            auto cameraBuffer = CameraBufferObject{};
            cameraBuffer.projection = info.cameraProjection;
            cameraBuffer.view = info.cameraView;
            memcpy(cameraUbos_[currentFrameIndex_].mappedMemory, &cameraBuffer, sizeof(cameraBuffer));

            auto directionalLightBuffer = DirectionalLightUboData{};
            directionalLightBuffer.direction = info.globalLightDirection;
            memcpy(directionalLightUbo_.mappedMemory, &directionalLightBuffer, sizeof(DirectionalLightUboData));

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
                                        info.skyboxHandle.value(),
                                        swapchain_.views[imageIndex]);
            geometryPass_->recordCommands(currentFrameIndex_,
                                          commandBuffer,
                                          meshVertexBuffer_.buffer,
                                          meshIndexBuffer_.buffer,
                                          meshGpuData_,
                                          materialGpuData_,
                                          swapchain_.views[imageIndex],
                                          info.drawCommands);

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

void Renderer::renderLoadingScreen(ImageHandle loadingScreenHandle)
{
    renderFrame(
        [this, &loadingScreenHandle](uint32_t imageIndex, const vk::raii::CommandBuffer& commandBuffer)
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

            loadingScreenPass_->recordCommands(commandBuffer, loadingScreenHandle, swapchain_.views[imageIndex]);

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
    emptyImage_.image = gpuDevice_.createImage(1, 1);
    emptyImage_.memory = gpuDevice_.allocateImageMemory(emptyImage_.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

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

    gpuDevice_.transitionImageLayout(*emptyImage_.image,
                                     *cmd,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     {}, // srcAccess
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::PipelineStageFlagBits2::eTopOfPipe,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::ImageAspectFlagBits::eColor);

    gpuDevice_.copyBufferToImage(cmd, stagingBuffer, emptyImage_.image, 1, 1);

    gpuDevice_.transitionImageLayout(*emptyImage_.image,
                                     *cmd,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal,
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::AccessFlagBits2::eShaderRead,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::ImageAspectFlagBits::eColor);

    emptyImage_.view = gpuDevice_.createImageView(emptyImage_.image);
    emptyImage_.sampler = gpuDevice_.createSampler();

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    // Meshes
    uploadMeshes(data.meshData);

    // Images
    imageGpuData_ = uploadImages(data.imageData);

    // Materials
    uploadMaterials(data.materialData);

    // Skyboxes
    uploadSkyboxes(data.skyboxData);

    skyboxPass_->regenerateDescriptorSets(cameraUbos_, skyboxGpuData_);
    geometryPass_->regenerateDescriptorSets(cameraUbos_, materialGpuData_, directionalLightGpuData_);
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
        auto cameraUbo = BufferObject{};
        cameraUbo.buffer = gpuDevice_.createUniformBuffer(sizeof(CameraBufferObject));
        cameraUbo.memory = gpuDevice_.allocateStagingBufferMemory(cameraUbo.buffer);
        cameraUbo.mappedMemory = cameraUbo.memory.mapMemory(0, VK_WHOLE_SIZE);

        cameraUbos_.push_back(std::move(cameraUbo));
    }
}

void Renderer::createDirectionalLightBuffers()
{
    directionalLightUbo_ = BufferObject{};
    directionalLightUbo_.buffer = gpuDevice_.createUniformBuffer(sizeof(DirectionalLightUboData));
    directionalLightUbo_.memory = gpuDevice_.allocateStagingBufferMemory(directionalLightUbo_.buffer);
    directionalLightUbo_.mappedMemory = directionalLightUbo_.memory.mapMemory(0, VK_WHOLE_SIZE);

    auto bufferInfo = vk::DescriptorBufferInfo{};
    bufferInfo.buffer = *directionalLightUbo_.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    directionalLightGpuData_.bufferInfo = bufferInfo;
}

void Renderer::createRenderPasses()
{
    const auto format = swapchain_.surfaceFormat.format;
    const auto extent = swapchain_.extent;

    loadingScreenPass_ = std::make_unique<LoadingScreenPass>(gpuDevice_, format, extent);
    skyboxPass_ = std::make_unique<SkyboxPass>(gpuDevice_, format, extent);
    geometryPass_ = std::make_unique<GeometryPass>(gpuDevice_, format, extent);
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

ImageContainer Renderer::uploadImages(const ImageDataContainer& data)
{
    auto container = ImageContainer{};
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

        container.emplace(handle, std::move(image));

        cmd.end();
        gpuDevice_.submitCommandBuffer(cmd);
    }

    return container;
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

    meshVertexBuffer_.buffer = gpuDevice_.createVertexBuffer(vertexBufferSize);
    meshVertexBuffer_.memory = gpuDevice_.allocateDeviceBufferMemory(meshVertexBuffer_.buffer);

    auto vertexStagingBuffer = gpuDevice_.createStagingBuffer(vertexBufferSize);
    auto vertexStagingBufferMemory = gpuDevice_.allocateStagingBufferMemory(vertexStagingBuffer);

    const auto indexBufferSize = static_cast<uint32_t>(sizeof(uint32_t) * totalIndices);
    meshIndexBuffer_.buffer = gpuDevice_.createIndexBuffer(indexBufferSize);
    meshIndexBuffer_.memory = gpuDevice_.allocateDeviceBufferMemory(meshIndexBuffer_.buffer);

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

    gpuDevice_.copyBuffer(cmd, vertexStagingBuffer, meshVertexBuffer_.buffer, vertexBufferSize);
    gpuDevice_.copyBuffer(cmd, indexStagingBuffer, meshIndexBuffer_.buffer, indexBufferSize);

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);
}

void Renderer::uploadMaterials(const MaterialDataContainer& data)
{
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    const auto uboStride = gpuDevice_.calculateAlignedUboStride(sizeof(MaterialUboData));

    materialUbo_.buffer = gpuDevice_.createUniformBuffer(uboStride * data.size());
    materialUbo_.memory = gpuDevice_.allocateStagingBufferMemory(materialUbo_.buffer);
    void* mapped = materialUbo_.memory.mapMemory(0, VK_WHOLE_SIZE);

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
        bufferInfo.buffer = *materialUbo_.buffer;
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
            imageInfo.imageView = *emptyImage_.view;
            imageInfo.sampler = *emptyImage_.sampler;
        }
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        material.imageInfo = imageInfo;

        materialGpuData_.emplace(handle, std::move(material));
    }

    materialUbo_.memory.unmapMemory();

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
        for (auto face = size_t{0}; face < 6; ++face)
        {
            auto dst = static_cast<uint8_t*>(mappedMemory) + (face * imageSize);
            std::memcpy(dst, skyboxData.imageData.at(face).data.data(), imageSize);
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
