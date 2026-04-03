#include "renderer/renderer.h"

#include "renderer/camera.h"
#include "renderer/pipeline.h"
#include "renderer/transition_barrier.h"
#include "renderer/vertex_layout.h"

#include <core/box.h>
#include <core/file_system.h>
#include <window/window.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

namespace renderer
{
struct CameraBufferObject
{
    glm::mat4 view;
    glm::mat4 projection;
};

struct ShadowPassPushConstants
{
    glm::mat4 modelTransform;
};

struct GeometryPassPushConstants
{
    glm::mat4 modelTransform;
    glm::mat4 normalMatrix;
};

constexpr auto maxFramesInFlight = uint32_t{2};
constexpr auto shadowMapSize = uint32_t{2048};

constexpr auto cameraDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex},
};
constexpr auto materialDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment},
    vk::DescriptorSetLayoutBinding{1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
};
constexpr auto directionalLightDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAll},
};
constexpr auto skyboxDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
};
constexpr auto loadingScreenImageDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
};
constexpr auto shadowMapImageDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
};

Renderer::Renderer(const window::Window& window)
    : instance_{context_, window.requiredExtensions()},
      surface_{window.createVulkanSurface(instance_.instance())},
      gpuDevice_{instance_.instance(), surface_},
      commandPool_{gpuDevice_.createCommandPool()},
      cameraDescriptor_{gpuDevice_.device(), cameraDescriptorBindings},
      materialDescriptor_{gpuDevice_.device(), materialDescriptorBindings},
      directionalLightDescriptor_{gpuDevice_.device(), directionalLightDescriptorBindings},
      skyboxDescriptor_{gpuDevice_.device(), skyboxDescriptorBindings},
      loadingScreenImageDescriptor_{gpuDevice_.device(), loadingScreenImageDescriptorBindings},
      shadowMapImageDescriptor_{gpuDevice_.device(), shadowMapImageDescriptorBindings}
{
    spdlog::info("Creating swapchain");
    createSwapchain();

    spdlog::info("Creating command buffers");
    createCommandBuffers();

    spdlog::info("Creating sync objects");
    createSyncObjects();

    spdlog::info("Creating samplers");
    createSamplers();

    spdlog::info("Creating default objects");
    emptyImage_ = std::make_unique<Image>(*gpuDevice_.device(),
                                          gpuDevice_.allocator(),
                                          vk::Extent3D{1, 1, 1},
                                          vk::Format::eR8G8B8A8Srgb);

    stageAndUploadImageData(emptyImage_->image(),
                            1,
                            1,
                            std::vector<std::byte>{std::byte{1}, std::byte{1}, std::byte{1}, std::byte{1}});

    shadowMapImage_ = std::make_unique<Image>(*gpuDevice_.device(),
                                              gpuDevice_.allocator(),
                                              vk::Extent3D{shadowMapSize, shadowMapSize, 1},
                                              vk::Format::eD32Sfloat);

    spdlog::info("Creating render passes");
    createCameraBuffers();
    createDirectionalLightBuffers();
    createShadowMapDescriptorSets();

    createShadowPass();
    createGeometryPass();
    createSkyboxPass();
    createLoadingScreenPass();
}

Renderer::~Renderer()
{
    gpuDevice_.device().waitIdle();
}

void Renderer::renderScene(const Camera& camera, const SceneDrawInfo& info)
{
    renderFrame(
        [this, &camera, &info](const vk::raii::CommandBuffer& commandBuffer)
        {
            auto cameraBuffer = CameraBufferObject{};
            cameraBuffer.projection = camera.projection();
            cameraBuffer.view = camera.view();
            cameraUniformBuffers_[currentFrameIndex_].write(&cameraBuffer, 0, sizeof(cameraBuffer));

            // Create a light box based on the current camera frustum (in world space).
            //
            // This is the area that will we do shadow calculations for. Later this can be extended add divided into
            // regions for cascading shadow maps, but for now its from the near plane extended to a suitable shadow
            // distance.
            const auto lightBox = camera.frustumSlice(camera.nearPlane, camera.nearPlane + shadowDistance_);

            // Create a light view matrix. We imagine the light the light looking at the center of our light box.
            //
            // Note that the light does not have a position, just a global direction, so pretend that it is far
            // away along in its direction (the actual distance doesn't matter too greatly as we'll use an
            // orthographic projection).
            const auto lightBoxCenter = lightBox.midPoint();
            const auto worldUp = glm::normalize(glm::vec3{0.0f, 1.0f, 0.0f});
            const auto lightDirection = glm::normalize(info.globalLightDirection);
            const auto lightPosition = lightBoxCenter - lightDirection * lightDistance_;
            const auto lightView = glm::lookAt(lightPosition, lightBoxCenter, worldUp);

            // Create a light projection matrix. This is an orthographic projection as we don't care about
            // perspective.
            //
            // We will use the world space light box transform to light space as the extent of the projection,
            // except since -Z is a forward direction, our near and far plane values need to be adjusted to positive
            // distances from the light (positive also as Vulkan will map to [0, 1] with
            // GLM_FORCE_DEPTH_ZERO_TO_ONE)
            const auto lightSpaceFrustum = viewTransform(lightBox, lightView);
            const auto lightSpaceFrustumAABB = lightSpaceFrustum.boudingBox();
            const auto near = -lightSpaceFrustumAABB.max.z;
            const auto far = -lightSpaceFrustumAABB.min.z;

            auto lightProjection = glm::ortho(lightSpaceFrustumAABB.min.x,
                                              lightSpaceFrustumAABB.max.x,
                                              lightSpaceFrustumAABB.min.y,
                                              lightSpaceFrustumAABB.max.y,
                                              near,
                                              far);

            // vulkan- y-flip, refactor out a central OrthoCamera to not have to remember to do this
            lightProjection[1][1] *= -1.0f;

            auto directionalLightBuffer = DirectionalLightUboData{};
            directionalLightBuffer.direction = info.globalLightDirection;
            directionalLightBuffer.lightSpaceView = lightView;
            directionalLightBuffer.lightSpaceProjection = lightProjection;

            directionalLightUniformBuffers_.at(currentFrameIndex_)
                .write(&directionalLightBuffer, 0, sizeof(DirectionalLightUboData));

            const auto viewport = vk::Viewport(0.0f,
                                               0.0f,
                                               static_cast<float>(swapchainExtent_.width),
                                               static_cast<float>(swapchainExtent_.height),
                                               0.0f,
                                               1.0f);

            /*
                The passes are here in a large block at the moment, as moving to separate classes was hiding too
               much. The intention is to break these down to be more scalable and data driven, as they are optimised
               and the flow of resources between passes better understood.
            */
            // Shadow pass
            {

                transitionImageLayout(shadowMapImage_->image(),
                                      commandBuffer,
                                      vk::ImageLayout::eUndefined,
                                      vk::ImageLayout::eDepthAttachmentOptimal,
                                      vk::ImageAspectFlagBits::eDepth);

                auto depthAttachmentInfo = vk::RenderingAttachmentInfo{};
                depthAttachmentInfo.imageView = shadowMapImage_->view();
                depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
                depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
                depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
                depthAttachmentInfo.clearValue = vk::ClearDepthStencilValue(1.0f, 0);

                auto renderingInfo = vk::RenderingInfo{};
                renderingInfo.renderArea = {.offset = {0, 0}, .extent = vk::Extent2D{shadowMapSize, shadowMapSize}};
                renderingInfo.layerCount = 1;
                renderingInfo.pDepthAttachment = &depthAttachmentInfo;

                commandBuffer.beginRendering(renderingInfo);
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *shadowPass_.pipeline);

                commandBuffer.setViewport(0,
                                          vk::Viewport(0.0f,
                                                       0.0f,
                                                       static_cast<float>(shadowMapSize),
                                                       static_cast<float>(shadowMapSize),
                                                       0.0f,
                                                       1.0f));
                commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D{shadowMapSize, shadowMapSize}));

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                 *shadowPass_.layout,
                                                 0,
                                                 *directionalLightDescriptorSets_.at(currentFrameIndex_),
                                                 nullptr);

                for (const auto& drawCommand : info.drawCommands)
                {
                    auto mesh = resources_.meshes.get(drawCommand.meshHandle);

                    auto vertexBuffer = resources_.buffers.get(mesh->vertexBuffer);
                    auto indexBuffer = resources_.buffers.get(mesh->indexBuffer);

                    commandBuffer.bindVertexBuffers(0, {vertexBuffer->buffer()}, {0});
                    commandBuffer.bindIndexBuffer(indexBuffer->buffer(), 0, vk::IndexType::eUint32);

                    auto pushConstants = ShadowPassPushConstants{};
                    pushConstants.modelTransform = drawCommand.transform;

                    commandBuffer.pushConstants(shadowPass_.layout,
                                                vk::ShaderStageFlagBits::eVertex,
                                                0,
                                                vk::ArrayProxy<const ShadowPassPushConstants>{pushConstants});

                    commandBuffer.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
                }

                commandBuffer.endRendering();

                transitionImageLayout(shadowMapImage_->image(),
                                      commandBuffer,
                                      vk::ImageLayout::eDepthAttachmentOptimal,
                                      vk::ImageLayout::eShaderReadOnlyOptimal,
                                      vk::ImageAspectFlagBits::eDepth);
            }

            transitionImageLayout(swapchainImages_.at(currentSwapchainImageIndex_),
                                  commandBuffer,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eColorAttachmentOptimal,
                                  vk::ImageAspectFlagBits::eColor);

            // Clear image pass
            {
                auto attachmentInfo = vk::RenderingAttachmentInfo{};
                attachmentInfo.imageView = swapchainImageViews_.at(currentSwapchainImageIndex_);
                attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
                attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
                attachmentInfo.clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};

                auto renderingInfo = vk::RenderingInfo{};
                renderingInfo.renderArea = {.offset = {0, 0}, .extent = swapchainExtent_};
                renderingInfo.layerCount = 1;
                renderingInfo.colorAttachmentCount = 1;
                renderingInfo.pColorAttachments = &attachmentInfo;

                commandBuffer.beginRendering(renderingInfo);
                commandBuffer.endRendering();
            }

            // Skybox pass (optional)
            if (info.skyboxHandle)
            {
                auto skybox = resources_.skyboxes.get(info.skyboxHandle.value());

                auto attachmentInfo = vk::RenderingAttachmentInfo{};
                attachmentInfo.imageView = swapchainImageViews_.at(currentSwapchainImageIndex_);
                attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                attachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
                attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

                auto renderingInfo = vk::RenderingInfo{};
                renderingInfo.renderArea = {.offset = {0, 0}, .extent = swapchainExtent_};
                renderingInfo.layerCount = 1;
                renderingInfo.colorAttachmentCount = 1;
                renderingInfo.pColorAttachments = &attachmentInfo;

                commandBuffer.beginRendering(renderingInfo);
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *skyboxPass_.pipeline);
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                 skyboxPass_.layout,
                                                 0,
                                                 *cameraDescriptorSets_.at(currentFrameIndex_),
                                                 nullptr);

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                 skyboxPass_.layout,
                                                 1,
                                                 *skybox->descriptorSet,
                                                 nullptr);

                commandBuffer.setViewport(0, viewport);
                commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent_));
                commandBuffer.draw(36, 1, 0, 0);
                commandBuffer.endRendering();
            }

            // Geometry pass
            {
                transitionImageLayout(depthTargetImage_->image(),
                                      commandBuffer,
                                      vk::ImageLayout::eUndefined,
                                      vk::ImageLayout::eDepthAttachmentOptimal,
                                      vk::ImageAspectFlagBits::eDepth);

                auto attachmentInfo = vk::RenderingAttachmentInfo{};
                attachmentInfo.imageView = swapchainImageViews_.at(currentSwapchainImageIndex_);
                attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                attachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
                attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

                auto depthAttachmentInfo = vk::RenderingAttachmentInfo{};
                depthAttachmentInfo.imageView = depthTargetImage_->view();
                depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
                depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
                depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eDontCare;
                depthAttachmentInfo.clearValue = vk::ClearDepthStencilValue(1.0f, 0);

                auto renderingInfo = vk::RenderingInfo{};
                renderingInfo.renderArea = {.offset = {0, 0}, .extent = swapchainExtent_};
                renderingInfo.layerCount = 1;
                renderingInfo.colorAttachmentCount = 1;
                renderingInfo.pColorAttachments = &attachmentInfo;
                renderingInfo.pDepthAttachment = &depthAttachmentInfo;

                commandBuffer.beginRendering(renderingInfo);
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *geometryPass_.pipeline);

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                 geometryPass_.layout,
                                                 0,
                                                 *cameraDescriptorSets_.at(currentFrameIndex_),
                                                 nullptr);

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                 *geometryPass_.layout,
                                                 2,
                                                 *directionalLightDescriptorSets_.at(currentFrameIndex_),
                                                 nullptr);

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                 *geometryPass_.layout,
                                                 3,
                                                 *shadowMapDescriptorSet_,
                                                 nullptr);

                commandBuffer.setViewport(0, viewport);
                commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent_));

                for (const auto& drawCommand : info.drawCommands)
                {
                    auto mesh = resources_.meshes.get(drawCommand.meshHandle);
                    auto material = resources_.materials.get(drawCommand.materialHandle);

                    auto vertexBuffer = resources_.buffers.get(mesh->vertexBuffer);
                    auto indexBuffer = resources_.buffers.get(mesh->indexBuffer);

                    commandBuffer.bindVertexBuffers(0, {vertexBuffer->buffer()}, {0});
                    commandBuffer.bindIndexBuffer(indexBuffer->buffer(), 0, vk::IndexType::eUint32);

                    auto pushConstants = GeometryPassPushConstants{};
                    pushConstants.modelTransform = drawCommand.transform;
                    pushConstants.normalMatrix = glm::transpose(glm::inverse(glm::mat3(drawCommand.transform)));

                    commandBuffer.pushConstants(geometryPass_.layout,
                                                vk::ShaderStageFlagBits::eVertex,
                                                0,
                                                vk::ArrayProxy<const GeometryPassPushConstants>{pushConstants});

                    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                     geometryPass_.layout,
                                                     1,
                                                     *material->descriptorSet,
                                                     nullptr);

                    commandBuffer.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
                }

                commandBuffer.endRendering();
            }

            transitionImageLayout(swapchainImages_.at(currentSwapchainImageIndex_),
                                  commandBuffer,
                                  vk::ImageLayout::eColorAttachmentOptimal,
                                  vk::ImageLayout::ePresentSrcKHR,
                                  vk::ImageAspectFlagBits::eColor);
        });
}

void Renderer::renderLoadingScreen(LoadingScreenHandle loadingScreenHandle)
{
    renderFrame(
        [this, &loadingScreenHandle](const vk::raii::CommandBuffer& commandBuffer)
        {
            transitionImageLayout(swapchainImages_.at(currentSwapchainImageIndex_),
                                  commandBuffer,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eColorAttachmentOptimal,
                                  vk::ImageAspectFlagBits::eColor);

            auto attachmentInfo = vk::RenderingAttachmentInfo{};
            attachmentInfo.imageView = swapchainImageViews_.at(currentSwapchainImageIndex_);
            attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
            attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
            attachmentInfo.clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};

            auto renderingInfo = vk::RenderingInfo{};
            renderingInfo.renderArea = {.offset = {0, 0}, .extent = swapchainExtent_};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &attachmentInfo;

            commandBuffer.beginRendering(renderingInfo);

            auto loadingScreen = resources_.loadingScreens.get(loadingScreenHandle);

            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *loadingScreenPass_.pipeline);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                             loadingScreenPass_.layout,
                                             0,
                                             *loadingScreen->descriptorSet,
                                             nullptr);

            commandBuffer.setViewport(0,
                                      vk::Viewport(0.0f,
                                                   0.0f,
                                                   static_cast<float>(swapchainExtent_.width),
                                                   static_cast<float>(swapchainExtent_.height),
                                                   0.0f,
                                                   1.0f));
            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent_));

            commandBuffer.draw(6, 1, 0, 0);

            commandBuffer.endRendering();

            transitionImageLayout(swapchainImages_.at(currentSwapchainImageIndex_),
                                  commandBuffer,
                                  vk::ImageLayout::eColorAttachmentOptimal,
                                  vk::ImageLayout::ePresentSrcKHR,
                                  vk::ImageAspectFlagBits::eColor);
        });
}

void Renderer::reset()
{
    gpuDevice_.device().waitIdle();

    // Free GPU data for reuse
    resources_.images.clear();
    resources_.materials.clear();
    resources_.meshes.clear();
    resources_.skyboxes.clear();
    resources_.buffers.clear();

    // Reset pools for per scene data
    materialDescriptor_.clear();
    skyboxDescriptor_.clear();
}

void Renderer::windowResized(int width, int height)
{
    if (width == 0 || height == 0)
    {
        windowMinimized_ = true;
    }
    else
    {
        windowMinimized_ = false;
    }

    swapchainRebuildRequired_ = true;
}

LoadingScreenHandle Renderer::addLoadingScreenImage(uint32_t width, uint32_t height, std::span<const std::byte> data)
{

    auto handle = resources_.loadingScreens.allocate();
    auto loadingScreen = resources_.loadingScreens.get(handle);
    loadingScreen->image = resources_.images.allocate(*gpuDevice_.device(),
                                                      gpuDevice_.allocator(),
                                                      vk::Extent3D{width, height, 1},
                                                      vk::Format::eR8G8B8A8Srgb);

    auto image = resources_.images.get(loadingScreen->image);
    stageAndUploadImageData(image->image(), width, height, data);

    loadingScreen->descriptorSet = std::move(loadingScreenImageDescriptor_.allocateSets(1)[0]);

    auto imageInfo = vk::DescriptorImageInfo{};
    imageInfo.imageView = image->view();
    imageInfo.sampler = imageSampler_;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    auto imageWrite = vk::WriteDescriptorSet{};
    imageWrite.dstSet = loadingScreen->descriptorSet;
    imageWrite.dstBinding = 0;
    imageWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    imageWrite.descriptorCount = 1;
    imageWrite.pImageInfo = &imageInfo;

    auto writes = std::array{imageWrite};
    gpuDevice_.device().updateDescriptorSets(writes, {});

    return handle;
}

ImageHandle Renderer::addImage(uint32_t width, uint32_t height, std::span<const std::byte> data)
{
    auto handle = resources_.images.allocate(*gpuDevice_.device(),
                                             gpuDevice_.allocator(),
                                             vk::Extent3D{width, height, 1},
                                             vk::Format::eR8G8B8A8Srgb);

    auto image = resources_.images.get(handle);
    stageAndUploadImageData(image->image(), width, height, data);

    return handle;
}

MaterialHandle Renderer::addMaterial(const MaterialData& data)
{
    auto handle = resources_.materials.allocate();
    auto material = resources_.materials.get(handle);

    const auto uboStride = gpuDevice_.calculateAlignedUboStride(sizeof(MaterialUboData));

    material->uniformBuffer = resources_.buffers.allocate(*gpuDevice_.device(),
                                                          gpuDevice_.allocator(),
                                                          uboStride,
                                                          Buffer::BufferType::Uniform);

    auto buffer = resources_.buffers.get(material->uniformBuffer);

    auto bufferData = MaterialUboData{};
    bufferData.diffuseColor = glm::vec4{data.diffuseColor, 1.0f};
    bufferData.hasDiffuseTexture = data.diffuseTexture ? 1 : 0;

    stageAndUploadBufferData(*buffer, &bufferData, 0, sizeof(MaterialUboData));

    // Create descriptor sets
    material->descriptorSet = std::move(materialDescriptor_.allocateSets(1)[0]);

    auto bufferInfo = vk::DescriptorBufferInfo{};
    bufferInfo.buffer = buffer->buffer();
    bufferInfo.offset = 0;
    bufferInfo.range = uboStride;

    auto uboWrite = vk::WriteDescriptorSet{};
    uboWrite.dstSet = *material->descriptorSet;
    uboWrite.dstBinding = 0;
    uboWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboWrite.descriptorCount = 1;
    uboWrite.pBufferInfo = &bufferInfo;

    // Tidy...
    auto imageView = emptyImage_->view();
    if (data.diffuseTexture)
    {
        imageView = resources_.images.get(data.diffuseTexture.value())->view();
    }

    auto imageInfo = vk::DescriptorImageInfo{};
    imageInfo.imageView = imageView;
    imageInfo.sampler = *imageSampler_;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    auto textureWrite = vk::WriteDescriptorSet{};
    textureWrite.dstSet = *material->descriptorSet;
    textureWrite.dstBinding = 1;
    textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    textureWrite.descriptorCount = 1;
    textureWrite.pImageInfo = &imageInfo;

    auto writes = std::array{uboWrite, textureWrite};
    gpuDevice_.device().updateDescriptorSets(writes, {});

    return handle;
}

MeshHandle Renderer::addMesh(std::span<const core::Vertex> vertices, std::span<const uint32_t> indices)
{
    auto handle = resources_.meshes.allocate();

    auto mesh = resources_.meshes.get(handle);
    mesh->vertexBuffer = resources_.buffers.allocate(*gpuDevice_.device(),
                                                     gpuDevice_.allocator(),
                                                     vertices.size_bytes(),
                                                     Buffer::BufferType::Vertex);

    mesh->indexBuffer = resources_.buffers.allocate(*gpuDevice_.device(),
                                                    gpuDevice_.allocator(),
                                                    indices.size_bytes(),
                                                    Buffer::BufferType::Index);

    mesh->indexCount = static_cast<uint32_t>(indices.size());

    stageAndUploadBufferData(*resources_.buffers.get(mesh->vertexBuffer), vertices.data(), 0, vertices.size_bytes());
    stageAndUploadBufferData(*resources_.buffers.get(mesh->indexBuffer), indices.data(), 0, indices.size_bytes());

    return handle;
}

// SkyboxHandle Renderer::addSkybox(const std::array<FaceData, 6>& data)
// {
//     auto handle = resources_.skyboxes.allocate();
//     auto skybox = resources_.skyboxes.get(handle);

//     auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
//     auto& cmd = commandBuffers[0];
//     cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

//     const auto width = data.at(0).width;
//     const auto height = data.at(0).height;
//     const auto imageSize = data.at(0).data.size_bytes();

//     skybox->image = gpuDevice_.createCubemapImage(width, height);
//     skybox->memory = gpuDevice_.allocateImageMemory(skybox->image, vk::MemoryPropertyFlagBits::eDeviceLocal);

//     auto stagingBuffer = gpuDevice_.createStagingBuffer(imageSize * 6);
//     auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

//     transitionImageLayout(*skybox->image,
//                           *cmd,
//                           vk::ImageLayout::eUndefined,
//                           vk::ImageLayout::eTransferDstOptimal,
//                           vk::ImageAspectFlagBits::eColor,
//                           6);

//     void* mappedMemory = stagingMemory.mapMemory(0, VK_WHOLE_SIZE);
//     for (auto face = size_t{0}; face < 6; ++face)
//     {
//         auto dst = static_cast<uint8_t*>(mappedMemory) + (face * imageSize);
//         std::memcpy(dst, data.at(face).data.data(), imageSize);
//     }
//     stagingMemory.unmapMemory();

//     gpuDevice_.copyBufferToImage(cmd, stagingBuffer, skybox->image, width, height, 6);

//     transitionImageLayout(*skybox->image,
//                           *cmd,
//                           vk::ImageLayout::eTransferDstOptimal,
//                           vk::ImageLayout::eShaderReadOnlyOptimal,
//                           vk::ImageAspectFlagBits::eColor,
//                           6);

//     skybox->view = gpuDevice_.createCubemapImageView(skybox->image);

//     cmd.end();
//     gpuDevice_.submitCommandBuffer(cmd);

//     skybox->descriptorSet = std::move(skyboxDescriptor_.allocateSets(1)[0]);

//     auto imageInfo = vk::DescriptorImageInfo{};
//     imageInfo.imageView = skybox->view;
//     imageInfo.sampler = imageSampler_;
//     imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

//     auto textureWrite = vk::WriteDescriptorSet{};
//     textureWrite.dstSet = skybox->descriptorSet;
//     textureWrite.dstBinding = 0;
//     textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
//     textureWrite.descriptorCount = 1;
//     textureWrite.pImageInfo = &imageInfo;

//     std::array writes{textureWrite};
//     gpuDevice_.device().updateDescriptorSets(writes, {});

//     return handle;
// }

void Renderer::createSwapchain()
{
    const auto surfaceCapabilities = gpuDevice_.physicalDevice().getSurfaceCapabilitiesKHR(*surface_);

    const auto availableSurfaceFormats = gpuDevice_.physicalDevice().getSurfaceFormatsKHR(*surface_);
    if (availableSurfaceFormats.empty())
    {
        throw std::runtime_error("No available surface formats");
    }

    auto surfaceFormat = availableSurfaceFormats.at(0);
    for (const auto& availableSurfaceFormat : availableSurfaceFormats)
    {
        if (availableSurfaceFormat.format == vk::Format::eB8G8R8A8Srgb
            && availableSurfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            surfaceFormat = availableSurfaceFormat;
            break;
        }
    }
    surfaceFormat_ = surfaceFormat.format;

    auto extent = vk::Extent2D{};
    if (surfaceCapabilities.currentExtent.width != 0xFFFFFFFF)
    {
        extent = surfaceCapabilities.currentExtent;
    }
    else
    {
        extent.width = std::clamp<uint32_t>(swapchainExtent_.width,
                                            surfaceCapabilities.minImageExtent.width,
                                            surfaceCapabilities.maxImageExtent.width);
        extent.height = std::clamp<uint32_t>(swapchainExtent_.height,
                                             surfaceCapabilities.minImageExtent.height,
                                             surfaceCapabilities.maxImageExtent.height);
    }
    swapchainExtent_ = extent;

    auto imageCount = uint32_t{0};
    if (surfaceCapabilities.maxImageCount == 0) // no maximum
    {
        imageCount = std::max(maxFramesInFlight, surfaceCapabilities.minImageCount);
    }
    else
    {
        imageCount = std::clamp(maxFramesInFlight,
                                surfaceCapabilities.minImageCount,
                                surfaceCapabilities.maxImageCount);
    }

    auto swapChainCreateInfo = vk::SwapchainCreateInfoKHR{};
    swapChainCreateInfo.surface = *surface_;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode = vk::PresentModeKHR::eFifo;
    swapChainCreateInfo.clipped = true;

    swapchain_ = vk::raii::SwapchainKHR(gpuDevice_.device(), swapChainCreateInfo);
    swapchainImages_ = swapchain_.getImages();

    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = surfaceFormat.format;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    for (const auto& image : swapchainImages_)
    {
        imageViewCreateInfo.image = image;
        swapchainImageViews_.emplace_back(gpuDevice_.device(), imageViewCreateInfo);
    }

    for (auto i = size_t{0}; i < imageCount; ++i)
    {
        renderFinishedSemaphores_.emplace_back(gpuDevice_.device(), vk::SemaphoreCreateInfo{});
    }

    for (auto i = size_t{0}; i < maxFramesInFlight; ++i)
    {
        presentCompleteSemaphores_.emplace_back(gpuDevice_.device(), vk::SemaphoreCreateInfo{});
    }
}

void Renderer::createCommandBuffers()
{
    commandBuffers_ = gpuDevice_.createCommandBuffers(commandPool_, maxFramesInFlight);
}

void Renderer::createSyncObjects()
{
    for (auto i = uint32_t{0}; i < maxFramesInFlight; ++i)
    {
        auto fenceCreateInfo = vk::FenceCreateInfo{};
        fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;
        drawFences_.emplace_back(gpuDevice_.device(), fenceCreateInfo);
    }
}

void Renderer::createSamplers()
{
    // Image sampler
    {
        auto samplerInfo = vk::SamplerCreateInfo{};
        samplerInfo.magFilter = vk::Filter::eNearest;
        samplerInfo.minFilter = vk::Filter::eNearest;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.anisotropyEnable = vk::False;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerInfo.unnormalizedCoordinates = vk::False;
        samplerInfo.compareEnable = vk::False;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

        imageSampler_ = vk::raii::Sampler(gpuDevice_.device(), samplerInfo);
    }

    // Shadow map sampler
    // Regular sampler with comparison enabled to allow sampler comparison in shader rather than us
    // performing depth comparisons manually
    {
        auto samplerInfo = vk::SamplerCreateInfo{};
        samplerInfo.magFilter = vk::Filter::eNearest;
        samplerInfo.minFilter = vk::Filter::eNearest;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.anisotropyEnable = vk::False;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
        samplerInfo.unnormalizedCoordinates = vk::False;
        samplerInfo.compareEnable = vk::True;
        samplerInfo.compareOp = vk::CompareOp::eGreater;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

        shadowSampler_ = vk::raii::Sampler(gpuDevice_.device(), samplerInfo);
    }
}

void Renderer::createCameraBuffers()
{
    for (auto frameIndex = uint32_t{0}; frameIndex < maxFramesInFlight; ++frameIndex)
    {
        auto buffer = Buffer(*gpuDevice_.device(),
                             gpuDevice_.allocator(),
                             sizeof(CameraBufferObject),
                             Buffer::BufferType::Uniform);
        auto set = std::move(cameraDescriptor_.allocateSets(1)[0]);

        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = buffer.buffer();
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        auto uboWrite = vk::WriteDescriptorSet{};
        uboWrite.dstSet = set;
        uboWrite.dstBinding = 0;
        uboWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        auto writes = std::array{uboWrite};
        gpuDevice_.device().updateDescriptorSets(writes, {});

        cameraUniformBuffers_.push_back(std::move(buffer));
        cameraDescriptorSets_.push_back(std::move(set));
    }
}

void Renderer::createDirectionalLightBuffers()
{
    for (auto frameIndex = uint32_t{0}; frameIndex < maxFramesInFlight; ++frameIndex)
    {
        auto buffer = Buffer{*gpuDevice_.device(),
                             gpuDevice_.allocator(),
                             sizeof(DirectionalLightUboData),
                             Buffer::BufferType::Uniform};

        auto set = std::move(directionalLightDescriptor_.allocateSets(1)[0]);

        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = buffer.buffer();
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        auto dirLightUboWrite = vk::WriteDescriptorSet{};
        dirLightUboWrite.dstSet = set;
        dirLightUboWrite.dstBinding = 0;
        dirLightUboWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        dirLightUboWrite.descriptorCount = 1;
        dirLightUboWrite.pBufferInfo = &bufferInfo;

        auto dirLightWrite = std::array{dirLightUboWrite};
        gpuDevice_.device().updateDescriptorSets(dirLightWrite, {});

        directionalLightUniformBuffers_.push_back(std::move(buffer));
        directionalLightDescriptorSets_.push_back(std::move(set));
    }
}

void Renderer::createShadowMapDescriptorSets()
{
    shadowMapDescriptorSet_ = std::move(shadowMapImageDescriptor_.allocateSets(1)[0]);

    auto imageInfo = vk::DescriptorImageInfo{};
    imageInfo.imageView = shadowMapImage_->view();
    imageInfo.sampler = *shadowSampler_;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    auto textureWrite = vk::WriteDescriptorSet{};
    textureWrite.dstSet = *shadowMapDescriptorSet_;
    textureWrite.dstBinding = 0;
    textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    textureWrite.descriptorCount = 1;
    textureWrite.pImageInfo = &imageInfo;

    auto writes = std::array{textureWrite};
    gpuDevice_.device().updateDescriptorSets(writes, {});
}

void Renderer::createShadowPass()
{
    auto pd = PipelineDesc{};
    pd.vertexShaderPath = core::getShaderDir() / "shadowmap.vert.spv";
    pd.fragmentShaderPath = core::getShaderDir() / "shadowmap.frag.spv";
    pd.vertexBindingDescriptions = {vertexBindingDescription};
    pd.vertexAttributeDescriptions = {vertexPositionAttribute};
    pd.descriptorLayouts = {*directionalLightDescriptor_.layout()};
    pd.pushConstantRanges = {
        vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(ShadowPassPushConstants)}};
    pd.depthAttachmentFormat = vk::Format::eD32Sfloat;
    pd.depthBiasEnable = vk::True;
    pd.depthBiasConstantFactor = 1.25f;
    pd.depthBiasSlopeFactor = 1.75f;
    pd.colourWriteMask = {};

    shadowPass_ = createPipeline(gpuDevice_.device(), gpuDevice_.physicalDevice(), pd);
}

void Renderer::createGeometryPass()
{
    auto pd = PipelineDesc{};
    pd.vertexShaderPath = core::getShaderDir() / "basic.vert.spv";
    pd.fragmentShaderPath = core::getShaderDir() / "basic.frag.spv";
    pd.vertexBindingDescriptions = {vertexBindingDescription};
    pd.vertexAttributeDescriptions = {vertexPositionAttribute, vertexNormalAttribute, vertexTextureUVAttribute};
    pd.descriptorLayouts = {*cameraDescriptor_.layout(),
                            *materialDescriptor_.layout(),
                            *directionalLightDescriptor_.layout(),
                            *shadowMapImageDescriptor_.layout()};
    pd.pushConstantRanges = {
        vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(GeometryPassPushConstants)}};
    pd.colorAttachmentFormats = {surfaceFormat_};
    pd.depthAttachmentFormat = vk::Format::eD32Sfloat;

    geometryPass_ = createPipeline(gpuDevice_.device(), gpuDevice_.physicalDevice(), pd);

    resizeGeometryPass();
}

void Renderer::createSkyboxPass()
{
    auto pd = PipelineDesc{};
    pd.vertexShaderPath = core::getShaderDir() / "skybox.vert.spv";
    pd.fragmentShaderPath = core::getShaderDir() / "skybox.frag.spv";
    pd.descriptorLayouts = {*cameraDescriptor_.layout(), *skyboxDescriptor_.layout()};
    pd.colorAttachmentFormats = {surfaceFormat_};
    pd.depthWriteEnable = vk::False;
    pd.cullMode = vk::CullModeFlagBits::eNone;

    skyboxPass_ = createPipeline(gpuDevice_.device(), gpuDevice_.physicalDevice(), pd);
}

void Renderer::createLoadingScreenPass()
{
    auto pd = PipelineDesc{};
    pd.vertexShaderPath = core::getShaderDir() / "loading_screen.vert.spv";
    pd.fragmentShaderPath = core::getShaderDir() / "loading_screen.frag.spv";
    pd.descriptorLayouts = {*loadingScreenImageDescriptor_.layout()};
    pd.colorAttachmentFormats = {surfaceFormat_};
    pd.depthTestEnable = vk::False;
    pd.depthWriteEnable = vk::False;
    pd.depthCompareOp = vk::CompareOp::eNever;
    pd.cullMode = vk::CullModeFlagBits::eNone;

    loadingScreenPass_ = createPipeline(gpuDevice_.device(), gpuDevice_.physicalDevice(), pd);
}

void Renderer::recreateSwapchain()
{
    if (windowMinimized_)
    {
        return;
    }

    gpuDevice_.device().waitIdle();

    swapchainImageViews_.clear();
    swapchainImages_.clear();
    presentCompleteSemaphores_.clear();
    renderFinishedSemaphores_.clear();
    swapchain_.clear();

    createSwapchain();

    resizeGeometryPass();

    swapchainRebuildRequired_ = false;
}

void Renderer::resizeGeometryPass()
{
    depthTargetImage_ = std::make_unique<Image>(*gpuDevice_.device(),
                                                gpuDevice_.allocator(),
                                                vk::Extent3D{swapchainExtent_, 1},
                                                vk::Format::eD32Sfloat);
}

void Renderer::renderFrame(std::function<void(const vk::raii::CommandBuffer&)> recordCommands)
{
    if (windowMinimized_)
    {
        return;
    }

    if (swapchainRebuildRequired_)
    {
        recreateSwapchain();
        return;
    }

    if (gpuDevice_.device().waitForFences(*drawFences_.at(currentFrameIndex_), vk::True, UINT64_MAX)
        != vk::Result::eSuccess)
    {
        throw std::runtime_error("Device unable to wait for fence to signal");
    }

    try
    {
        auto result = vk::Result{};

        std::tie(result, currentSwapchainImageIndex_) = swapchain_.acquireNextImage(
            std::numeric_limits<uint64_t>::max(),
            presentCompleteSemaphores_.at(currentFrameIndex_),
            nullptr);

        if (result == vk::Result::eSuboptimalKHR)
        {
            swapchainRebuildRequired_ = true;
        }
    }
    catch (const vk::OutOfDateKHRError&)
    {
        swapchainRebuildRequired_ = true;
        return; // cannot continue - we did not acquire a swapchain image so try again next frame
    }

    auto& commandBuffer = commandBuffers_.at(currentFrameIndex_);
    commandBuffer.reset();
    commandBuffer.begin({});

    recordCommands(commandBuffer);

    commandBuffer.end();

    auto waitSemaphores = std::array{*presentCompleteSemaphores_.at(currentFrameIndex_)};
    auto signalSemaphores = std::array{*renderFinishedSemaphores_.at(currentSwapchainImageIndex_)};

    gpuDevice_.device().resetFences(*drawFences_.at(currentFrameIndex_));
    gpuDevice_.submitCommandBuffer(commandBuffer,
                                   waitSemaphores,
                                   vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput),
                                   signalSemaphores,
                                   *drawFences_.at(currentFrameIndex_));

    auto presentInfo = vk::PresentInfoKHR{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &*renderFinishedSemaphores_.at(currentSwapchainImageIndex_);
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &*swapchain_;
    presentInfo.pImageIndices = &currentSwapchainImageIndex_;

    try
    {
        const auto result = gpuDevice_.presentQueue().presentKHR(presentInfo);
        if (result == vk::Result::eSuboptimalKHR)
        {
            swapchainRebuildRequired_ = true;
        }
    }
    catch (const vk::OutOfDateKHRError&)
    {
        swapchainRebuildRequired_ = true;
    }

    currentFrameIndex_ = (currentFrameIndex_ + 1) % maxFramesInFlight;
}

void Renderer::stageAndUploadBufferData(Buffer& buffer, const void* data, size_t offset, size_t size)
{
    if (buffer.isHostVisible()) // Skip staging
    {
        buffer.write(data, offset, size);
    }
    else
    {
        auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
        auto& cmd = commandBuffers[0];
        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        auto stagingBuffer = Buffer{*gpuDevice_.device(), gpuDevice_.allocator(), size, Buffer::BufferType::Staging};

        stagingBuffer.write(data, offset, size);

        cmd.copyBuffer(stagingBuffer.buffer(), buffer.buffer(), vk::BufferCopy(0, 0, size));

        cmd.end();
        gpuDevice_.submitCommandBuffer(cmd);
    }
}

void Renderer::stageAndUploadImageData(VkImage image, uint32_t width, uint32_t height, std::span<const std::byte> data)
{
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto stagingBuffer =
        Buffer{*gpuDevice_.device(), gpuDevice_.allocator(), data.size_bytes(), Buffer::BufferType::Staging};

    stagingBuffer.write(data.data(), 0, data.size_bytes());

    transitionImageLayout(image,
                          *cmd,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageAspectFlagBits::eColor);

    auto region = vk::BufferImageCopy{};
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = vk::Extent3D{width, height, 1};

    cmd.copyBufferToImage(stagingBuffer.buffer(), image, vk::ImageLayout::eTransferDstOptimal, region);

    transitionImageLayout(image,
                          *cmd,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageAspectFlagBits::eColor);

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);
}
} // namespace renderer
