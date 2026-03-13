#include "renderer/renderer.h"

#include "render_passes/geometry_pass.h"
#include "render_passes/loading_screen_pass.h"
#include "render_passes/shadow_map_pass.h"
#include "render_passes/skybox_pass.h"
#include "renderer/camera.h"
#include "renderer/data.h"

#include <core/box.h>
#include <window/window.h>

#include <spdlog/spdlog.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

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
      swapchain_{gpuDevice_.device(), gpuDevice_.physicalDevice(), surface_},
      windowExtent_{static_cast<uint32_t>(window.width()), static_cast<uint32_t>(window.height())},
      commandPool_{gpuDevice_.createCommandPool()}
{
    spdlog::info("Creating swapchain");
    swapchain_.initialize(maxFramesInFlight, windowExtent_);

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
    resources_.loadingScreens = uploadImages(imageData);

    loadingScreenPass_->regenerateDescriptorSets(resources_.loadingScreens);
}

void Renderer::renderScene(const Camera& camera, const SceneDrawInfo& info)
{
    renderFrame(
        [this, &camera, &info](const vk::raii::CommandBuffer& commandBuffer)
        {
            auto cameraBuffer = CameraBufferObject{};
            cameraBuffer.projection = camera.projection();
            cameraBuffer.view = camera.view();
            memcpy(buffers_.cameraBuffers[currentFrameIndex_].mappedMemory, &cameraBuffer, sizeof(cameraBuffer));

            const auto shadowDistance = 50.0f;
            const auto cameraFrustum = camera.frustumSlice(camera.nearPlane, camera.nearPlane + shadowDistance);
            const auto cameraFrustumMidPoint = cameraFrustum.midPoint();

            const auto up = std::abs(info.globalLightDirection.y) > 0.99f ? glm::vec3{0, 0, 1} : glm::vec3{0, 1, 0};

            // "Pretend" the directional light is far away along its -direction
            const auto mimicLightPosition = cameraFrustumMidPoint - info.globalLightDirection * shadowDistance;
            const auto lightRight = glm::normalize(glm::cross(info.globalLightDirection, up));
            const auto lightUp = glm::normalize(glm::cross(info.globalLightDirection, lightRight));

            const auto lightViewMatrix = glm::lookAt(mimicLightPosition, cameraFrustumMidPoint, lightUp);
            const auto lightSpaceFrustum = viewTransform(cameraFrustum, lightViewMatrix);
            const auto frustumAABB = lightSpaceFrustum.boudingBox();

            // near and far plane reversed for light space
            const auto near = -frustumAABB.max.z;
            const auto far = -frustumAABB.min.z;

            const auto lightProjectionMatrix =
                glm::ortho(frustumAABB.min.x, frustumAABB.max.x, frustumAABB.min.y, frustumAABB.max.y, near, far);

            auto directionalLightBuffer = DirectionalLightUboData{};
            directionalLightBuffer.direction = info.globalLightDirection;
            directionalLightBuffer.lightSpaceView = lightViewMatrix;
            directionalLightBuffer.lightSpaceProjection = lightProjectionMatrix;
            memcpy(buffers_.directionalLightBuffer.mappedMemory,
                   &directionalLightBuffer,
                   sizeof(DirectionalLightUboData));

            gpuDevice_.transitionImageLayout(swapchain_.currentImage(),
                                             commandBuffer,
                                             vk::ImageLayout::eUndefined,
                                             vk::ImageLayout::eColorAttachmentOptimal,
                                             {},
                                             vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                                             vk::ImageAspectFlagBits::eColor);

            shadowMapPass_->recordCommands(commandBuffer, buffers_, resources_, info.drawCommands);

            skyboxPass_->recordCommands(currentFrameIndex_,
                                        commandBuffer,
                                        info.skyboxHandle.value(),
                                        swapchain_.currentImageView());

            geometryPass_->recordCommands(currentFrameIndex_,
                                          commandBuffer,
                                          buffers_,
                                          resources_,
                                          swapchain_.currentImageView(),
                                          info.drawCommands);

            gpuDevice_.transitionImageLayout(swapchain_.currentImage(),
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
        [this, &loadingScreenHandle](const vk::raii::CommandBuffer& commandBuffer)
        {
            gpuDevice_.transitionImageLayout(swapchain_.currentImage(),
                                             commandBuffer,
                                             vk::ImageLayout::eUndefined,
                                             vk::ImageLayout::eColorAttachmentOptimal,
                                             {},
                                             vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                                             vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                                             vk::ImageAspectFlagBits::eColor);

            loadingScreenPass_->recordCommands(commandBuffer, loadingScreenHandle, swapchain_.currentImageView());

            gpuDevice_.transitionImageLayout(swapchain_.currentImage(),
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
    windowExtent_.width = static_cast<uint32_t>(width);
    windowExtent_.height = static_cast<uint32_t>(height);

    if (width == 0 && height == 0)
    {
        windowMinimized_ = true;
    }
    else
    {
        windowMinimized_ = false;
    }

    swapchain_.markOutOfDate();
}

void Renderer::setData(const AssetData& data)
{
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Empty image (move this)
    resources_.emptyImage.image = gpuDevice_.createImage(1, 1);
    resources_.emptyImage.memory = gpuDevice_.allocateImageMemory(resources_.emptyImage.image,
                                                                  vk::MemoryPropertyFlagBits::eDeviceLocal);

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

    gpuDevice_.transitionImageLayout(*resources_.emptyImage.image,
                                     *cmd,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     {}, // srcAccess
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::PipelineStageFlagBits2::eTopOfPipe,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::ImageAspectFlagBits::eColor);

    gpuDevice_.copyBufferToImage(cmd, stagingBuffer, resources_.emptyImage.image, 1, 1);

    gpuDevice_.transitionImageLayout(*resources_.emptyImage.image,
                                     *cmd,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal,
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::AccessFlagBits2::eShaderRead,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::ImageAspectFlagBits::eColor);

    resources_.emptyImage.view = gpuDevice_.createImageView(resources_.emptyImage.image);
    resources_.emptyImage.sampler = gpuDevice_.createSampler();

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    // Meshes
    uploadMeshes(data.meshData);

    // Images
    resources_.images = uploadImages(data.imageData);

    // Materials
    uploadMaterials(data.materialData);

    // Skyboxes
    uploadSkyboxes(data.skyboxData);

    skyboxPass_->regenerateDescriptorSets(buffers_, resources_);
    geometryPass_->regenerateDescriptorSets(buffers_, resources_);
    shadowMapPass_->regenerateDescriptorSets(buffers_);
}

void Renderer::createCommandBuffers()
{
    commandBuffers_ = gpuDevice_.createCommandBuffers(commandPool_, maxFramesInFlight);
}

void Renderer::createSyncObjects()
{
    for ([[maybe_unused]] auto _ : std::views::repeat(0, maxFramesInFlight))
    {
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

        buffers_.cameraBuffers.push_back(std::move(cameraUbo));
    }
}

void Renderer::createDirectionalLightBuffers()
{
    buffers_.directionalLightBuffer.buffer = gpuDevice_.createUniformBuffer(sizeof(DirectionalLightUboData));
    buffers_.directionalLightBuffer.memory = gpuDevice_.allocateStagingBufferMemory(
        buffers_.directionalLightBuffer.buffer);
    buffers_.directionalLightBuffer.mappedMemory = buffers_.directionalLightBuffer.memory.mapMemory(0, VK_WHOLE_SIZE);
}

void Renderer::createRenderPasses()
{
    const auto format = swapchain_.imageFormat();
    const auto extent = swapchain_.extent();

    loadingScreenPass_ = std::make_unique<LoadingScreenPass>(gpuDevice_, format, extent);
    skyboxPass_ = std::make_unique<SkyboxPass>(gpuDevice_, format, extent);
    geometryPass_ = std::make_unique<GeometryPass>(gpuDevice_, format, extent);
    shadowMapPass_ = std::make_unique<ShadowMapPass>(gpuDevice_);
}

void Renderer::renderFrame(std::function<void(const vk::raii::CommandBuffer&)> recordCommands)
{
    if (gpuDevice_.device().waitForFences(*drawFences_.at(currentFrameIndex_), vk::True, UINT64_MAX)
        != vk::Result::eSuccess)
    {
        throw std::runtime_error("Device unable to wait for fence to signal");
    }

    if (windowMinimized_)
    {
        return;
    }

    if (swapchain_.outOfDate())
    {
        gpuDevice_.device().waitIdle();
        swapchain_.initialize(maxFramesInFlight, windowExtent_);

        loadingScreenPass_->resize(swapchain_.extent());
        skyboxPass_->resize(swapchain_.extent());
        geometryPass_->resize(swapchain_.extent());
    }

    if (!swapchain_.acquireNextImage())
    {
        return;
    }

    auto& commandBuffer = commandBuffers_.at(currentFrameIndex_);
    commandBuffer.reset();
    commandBuffer.begin({});

    recordCommands(commandBuffer);

    commandBuffer.end();

    auto waitSemaphores = std::array{*swapchain_.currentPresentCompleteSemaphore()};
    auto signalSemaphores = std::array{*swapchain_.currentRenderFinishedSemaphore()};

    gpuDevice_.device().resetFences(*drawFences_.at(currentFrameIndex_));
    gpuDevice_.submitCommandBuffer(commandBuffer,
                                   waitSemaphores,
                                   vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput),
                                   signalSemaphores,
                                   *drawFences_.at(currentFrameIndex_));

    swapchain_.present(gpuDevice_.presentQueue());

    gpuDevice_.device().waitIdle();

    currentFrameIndex_ = (currentFrameIndex_ + 1) % maxFramesInFlight;
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

    buffers_.meshVertexBuffer.buffer = gpuDevice_.createVertexBuffer(vertexBufferSize);
    buffers_.meshVertexBuffer.memory = gpuDevice_.allocateDeviceBufferMemory(buffers_.meshVertexBuffer.buffer);

    auto vertexStagingBuffer = gpuDevice_.createStagingBuffer(vertexBufferSize);
    auto vertexStagingBufferMemory = gpuDevice_.allocateStagingBufferMemory(vertexStagingBuffer);

    const auto indexBufferSize = static_cast<uint32_t>(sizeof(uint32_t) * totalIndices);
    buffers_.meshIndexBuffer.buffer = gpuDevice_.createIndexBuffer(indexBufferSize);
    buffers_.meshIndexBuffer.memory = gpuDevice_.allocateDeviceBufferMemory(buffers_.meshIndexBuffer.buffer);

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

        resources_.meshes.emplace(handle, std::move(mesh));
    }

    vertexStagingBufferMemory.unmapMemory();
    indexStagingBufferMemory.unmapMemory();

    gpuDevice_.copyBuffer(cmd, vertexStagingBuffer, buffers_.meshVertexBuffer.buffer, vertexBufferSize);
    gpuDevice_.copyBuffer(cmd, indexStagingBuffer, buffers_.meshIndexBuffer.buffer, indexBufferSize);

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);
}

void Renderer::uploadMaterials(const MaterialDataContainer& data)
{
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    const auto uboStride = gpuDevice_.calculateAlignedUboStride(sizeof(MaterialUboData));

    buffers_.materialBuffer.buffer = gpuDevice_.createUniformBuffer(uboStride * data.size());
    buffers_.materialBuffer.memory = gpuDevice_.allocateStagingBufferMemory(buffers_.materialBuffer.buffer);
    void* mapped = buffers_.materialBuffer.memory.mapMemory(0, VK_WHOLE_SIZE);

    auto offset = uint32_t{0};
    for (const auto& [handle, materialData] : data)
    {
        auto material = Material{};
        material.bufferOffset = offset;
        material.size = uboStride;
        material.diffuseImageHandle = materialData.diffuseImage;

        auto bufferData = MaterialUboData{};
        bufferData.diffuseColor = glm::vec4{materialData.diffuseColour, 1.0f};
        bufferData.hasDiffuseTexture = materialData.diffuseImage ? 1 : 0;
        std::memcpy(static_cast<std::byte*>(mapped) + offset, &bufferData, sizeof(MaterialUboData));

        offset += static_cast<uint32_t>(uboStride);

        resources_.materials.emplace(handle, std::move(material));
    }

    buffers_.materialBuffer.memory.unmapMemory();

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

        resources_.skyboxes.emplace(handle, std::move(skybox));

        cmd.end();
        gpuDevice_.submitCommandBuffer(cmd);
    }
}

} // namespace renderer
