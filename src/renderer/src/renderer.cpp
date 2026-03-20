#include "renderer/renderer.h"

#include "renderer/camera.h"
#include "renderer/pipeline.h"
#include "renderer/transition_barrier.h"
#include "renderer/vertex_layout.h"

#include <core/box.h>
#include <core/file_system.h>
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

struct ShadowPassPushConstants
{
    glm::mat4 modelTransform;
};

struct GeometryPassPushConstants
{
    glm::mat4 modelTransform;
    glm::mat4 normalMatrix;
};

constexpr auto maxFramesInFlight = 2;
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

// Move this
Image createImage(const GpuDevice& gpuDevice,
                  const vk::raii::CommandPool& commandPool,
                  uint32_t width,
                  uint32_t height,
                  std::span<const std::byte> data)
{

    auto commandBuffers = gpuDevice.createCommandBuffers(commandPool, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto image = Image{};
    image.image = gpuDevice.createImage(width, height);
    image.memory = gpuDevice.allocateImageMemory(image.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto stagingBuffer = gpuDevice.createStagingBuffer(data.size_bytes());
    auto stagingMemory = gpuDevice.allocateStagingBufferMemory(stagingBuffer);

    void* mappedMemory = stagingMemory.mapMemory(0, VK_WHOLE_SIZE);
    std::memcpy(mappedMemory, data.data(), data.size_bytes());
    stagingMemory.unmapMemory();

    transitionImageLayout(*image.image,
                          *cmd,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageAspectFlagBits::eColor);

    gpuDevice.copyBufferToImage(cmd, stagingBuffer, image.image, width, height);

    transitionImageLayout(*image.image,
                          *cmd,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageAspectFlagBits::eColor);

    image.view = gpuDevice.createImageView(image.image);

    cmd.end();
    gpuDevice.submitCommandBuffer(cmd);

    return image;
}

Renderer::Renderer(const window::Window& window)
    : instance_{context_, window.requiredExtensions()},
      surface_{window.createVulkanSurface(instance_.instance())},
      gpuDevice_{instance_.instance(), surface_},
      swapchain_{gpuDevice_.device(), gpuDevice_.physicalDevice(), surface_},
      windowExtent_{static_cast<uint32_t>(window.width()), static_cast<uint32_t>(window.height())},
      commandPool_{gpuDevice_.createCommandPool()},
      cameraDescriptor_{gpuDevice_.device(), cameraDescriptorBindings},
      materialDescriptor_{gpuDevice_.device(), materialDescriptorBindings},
      directionalLightDescriptor_{gpuDevice_.device(), directionalLightDescriptorBindings},
      skyboxDescriptor_{gpuDevice_.device(), skyboxDescriptorBindings},
      loadingScreenImageDescriptor_{gpuDevice_.device(), loadingScreenImageDescriptorBindings},
      shadowMapImageDescriptor_{gpuDevice_.device(), shadowMapImageDescriptorBindings},
      imageSampler_{gpuDevice_.createSampler()}
{
    spdlog::info("Creating swapchain");
    swapchain_.initialize(maxFramesInFlight, windowExtent_);

    spdlog::info("Creating command buffers");
    createCommandBuffers();

    spdlog::info("Creating sync objects");
    createSyncObjects();

    spdlog::info("Creating default objects");
    emptyImage_ = createImage(gpuDevice_,
                              commandPool_,
                              1,
                              1,
                              std::vector<std::byte>{std::byte{1}, std::byte{1}, std::byte{1}, std::byte{1}});

    shadowMapImage_ = gpuDevice_.createDepthImage(shadowMapSize, shadowMapSize);
    shadowMapImageMemory_ = gpuDevice_.allocateImageMemory(shadowMapImage_, vk::MemoryPropertyFlagBits::eDeviceLocal);
    shadowMapImageView_ = gpuDevice_.createDepthImageView(shadowMapImage_);

    spdlog::info("Creating render passes");
    createCameraBuffers();
    createDirectionalLightBuffers();
    // createShadowMapDescriptorSets();

    createShadowPass();
    createGeometryPass();
    createSkyboxPass();
    createLoadingScreenPass();
}

Renderer::~Renderer() = default;

void Renderer::renderScene(const Camera& camera, const SceneDrawInfo& info)
{
    renderFrame(
        [this, &camera, &info](const vk::raii::CommandBuffer& commandBuffer)
        {
            auto cameraBuffer = CameraBufferObject{};
            cameraBuffer.projection = camera.projection();
            cameraBuffer.view = camera.view();
            memcpy(cameraUniformBuffersMappedMemory_[currentFrameIndex_], &cameraBuffer, sizeof(cameraBuffer));

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
            memcpy(directionalLightUniformBufferMappedMemory_,
                   &directionalLightBuffer,
                   sizeof(DirectionalLightUboData));

            const auto viewport = vk::Viewport(0.0f,
                                               0.0f,
                                               static_cast<float>(windowExtent_.width),
                                               static_cast<float>(windowExtent_.height),
                                               0.0f,
                                               1.0f);

            /*
            The passes are here in a large block at the moment, as moving to separate classes was hiding too much.
            The intention is to break these down to be more scalable and data driven, as they are optimised and the flow
            of resources between passes better understood.
            */
            // Shadow pass
            {

                transitionImageLayout(shadowMapImage_,
                                      commandBuffer,
                                      vk::ImageLayout::eUndefined,
                                      vk::ImageLayout::eDepthAttachmentOptimal,
                                      vk::ImageAspectFlagBits::eDepth);

                auto depthAttachmentInfo = vk::RenderingAttachmentInfo{};
                depthAttachmentInfo.imageView = shadowMapImageView_;
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
                                                 *directionalLightDescriptorSet_,
                                                 nullptr);

                for (const auto& drawCommand : info.drawCommands)
                {
                    auto mesh = resources_.meshes.get(drawCommand.meshHandle);

                    commandBuffer.bindVertexBuffers(0, *mesh->vertexBuffer, {0});
                    commandBuffer.bindIndexBuffer(*mesh->indexBuffer, 0, vk::IndexType::eUint32);

                    auto pushConstants = ShadowPassPushConstants{};
                    pushConstants.modelTransform = drawCommand.transform;

                    commandBuffer.pushConstants(shadowPass_.layout,
                                                vk::ShaderStageFlagBits::eVertex,
                                                0,
                                                vk::ArrayProxy<const ShadowPassPushConstants>{pushConstants});

                    commandBuffer.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
                }

                commandBuffer.endRendering();

                // transitionImageLayout(shadowMapImage_,
                //                       commandBuffer,
                //                       vk::ImageLayout::eDepthAttachmentOptimal,
                //                       vk::ImageLayout::eShaderReadOnlyOptimal,
                //                       vk::ImageAspectFlagBits::eDepth);
            }

            transitionImageLayout(swapchain_.currentImage(),
                                  commandBuffer,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eColorAttachmentOptimal,
                                  vk::ImageAspectFlagBits::eColor);

            // Clear image pass
            {
                auto attachmentInfo = vk::RenderingAttachmentInfo{};
                attachmentInfo.imageView = swapchain_.currentImageView();
                attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
                attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
                attachmentInfo.clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 1.0f, 0.0f, 1.0f}};

                auto renderingInfo = vk::RenderingInfo{};
                renderingInfo.renderArea = {.offset = {0, 0}, .extent = windowExtent_};
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
                attachmentInfo.imageView = swapchain_.currentImageView();
                attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                attachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
                attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

                auto renderingInfo = vk::RenderingInfo{};
                renderingInfo.renderArea = {.offset = {0, 0}, .extent = windowExtent_};
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
                commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), windowExtent_));
                commandBuffer.draw(36, 1, 0, 0);
                commandBuffer.endRendering();
            }

            // Geometry pass
            {
                transitionImageLayout(depthTargetImage_,
                                      commandBuffer,
                                      vk::ImageLayout::eUndefined,
                                      vk::ImageLayout::eDepthAttachmentOptimal,
                                      vk::ImageAspectFlagBits::eDepth);

                auto attachmentInfo = vk::RenderingAttachmentInfo{};
                attachmentInfo.imageView = swapchain_.currentImageView();
                attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                attachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
                attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

                auto depthAttachmentInfo = vk::RenderingAttachmentInfo{};
                depthAttachmentInfo.imageView = depthTargetImageView_;
                depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
                depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
                depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eDontCare;
                depthAttachmentInfo.clearValue = vk::ClearDepthStencilValue(1.0f, 0);

                auto renderingInfo = vk::RenderingInfo{};
                renderingInfo.renderArea = {.offset = {0, 0}, .extent = windowExtent_};
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
                                                 *directionalLightDescriptorSet_,
                                                 nullptr);

                commandBuffer.setViewport(0, viewport);
                commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), windowExtent_));

                for (const auto& drawCommand : info.drawCommands)
                {
                    auto mesh = resources_.meshes.get(drawCommand.meshHandle);
                    auto material = resources_.materials.get(drawCommand.materialHandle);

                    commandBuffer.bindVertexBuffers(0, *mesh->vertexBuffer, {0});
                    commandBuffer.bindIndexBuffer(*mesh->indexBuffer, 0, vk::IndexType::eUint32);

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

            transitionImageLayout(swapchain_.currentImage(),
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
            transitionImageLayout(swapchain_.currentImage(),
                                  commandBuffer,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eColorAttachmentOptimal,
                                  vk::ImageAspectFlagBits::eColor);

            auto attachmentInfo = vk::RenderingAttachmentInfo{};
            attachmentInfo.imageView = swapchain_.currentImageView();
            attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
            attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
            attachmentInfo.clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};

            auto renderingInfo = vk::RenderingInfo{};
            renderingInfo.renderArea = {.offset = {0, 0}, .extent = windowExtent_};
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
                                                   static_cast<float>(windowExtent_.width),
                                                   static_cast<float>(windowExtent_.height),
                                                   0.0f,
                                                   1.0f));
            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), windowExtent_));

            commandBuffer.draw(6, 1, 0, 0);

            commandBuffer.endRendering();

            transitionImageLayout(swapchain_.currentImage(),
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

    // Reset pools for per scene data
    materialDescriptor_.clear();
    skyboxDescriptor_.clear();
}

void Renderer::windowResized(int width, int height)
{
    spdlog::info("Renderer resized {} x {}", width, height);

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

LoadingScreenHandle Renderer::addLoadingScreenImage(uint32_t width, uint32_t height, std::span<const std::byte> data)
{
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto handle = resources_.loadingScreens.allocate();
    auto loadingScreen = resources_.loadingScreens.get(handle);
    loadingScreen->image = gpuDevice_.createImage(width, height);
    loadingScreen->memory = gpuDevice_.allocateImageMemory(loadingScreen->image,
                                                           vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto stagingBuffer = gpuDevice_.createStagingBuffer(data.size_bytes());
    auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

    void* mappedMemory = stagingMemory.mapMemory(0, VK_WHOLE_SIZE);
    std::memcpy(mappedMemory, data.data(), data.size_bytes());
    stagingMemory.unmapMemory();

    transitionImageLayout(*loadingScreen->image,
                          *cmd,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageAspectFlagBits::eColor);

    gpuDevice_.copyBufferToImage(cmd, stagingBuffer, loadingScreen->image, width, height);

    transitionImageLayout(*loadingScreen->image,
                          *cmd,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageAspectFlagBits::eColor);

    loadingScreen->view = gpuDevice_.createImageView(loadingScreen->image);

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    loadingScreen->descriptorSet = std::move(loadingScreenImageDescriptor_.allocateSets(1)[0]);

    auto imageInfo = vk::DescriptorImageInfo{};
    imageInfo.imageView = loadingScreen->view;
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
    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    auto handle = resources_.images.allocate();
    auto image = resources_.images.get(handle);
    image->image = gpuDevice_.createImage(width, height);
    image->memory = gpuDevice_.allocateImageMemory(image->image, vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto stagingBuffer = gpuDevice_.createStagingBuffer(data.size_bytes());
    auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

    void* mappedMemory = stagingMemory.mapMemory(0, VK_WHOLE_SIZE);
    std::memcpy(mappedMemory, data.data(), data.size_bytes());
    stagingMemory.unmapMemory();

    transitionImageLayout(*image->image,
                          *cmd,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageAspectFlagBits::eColor);

    gpuDevice_.copyBufferToImage(cmd, stagingBuffer, image->image, width, height);

    transitionImageLayout(*image->image,
                          *cmd,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageAspectFlagBits::eColor);

    image->view = gpuDevice_.createImageView(image->image);

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    return handle;
}

MaterialHandle Renderer::addMaterial(const MaterialData& data)
{
    auto handle = resources_.materials.allocate();
    auto material = resources_.materials.get(handle);

    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    const auto uboStride = gpuDevice_.calculateAlignedUboStride(sizeof(MaterialUboData));

    material->uniformBuffer = gpuDevice_.createUniformBuffer(uboStride);
    material->uniformBufferMemory = gpuDevice_.allocateStagingBufferMemory(material->uniformBuffer);
    void* mapped = material->uniformBufferMemory.mapMemory(0, VK_WHOLE_SIZE);

    auto bufferData = MaterialUboData{};
    bufferData.diffuseColor = glm::vec4{data.diffuseColor, 1.0f};
    bufferData.hasDiffuseTexture = data.diffuseTexture ? 1 : 0;
    std::memcpy(mapped, &bufferData, sizeof(MaterialUboData));

    material->uniformBufferMemory.unmapMemory();

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    // Create descriptor sets
    material->descriptorSet = std::move(materialDescriptor_.allocateSets(1)[0]);

    auto bufferInfo = vk::DescriptorBufferInfo{};
    bufferInfo.buffer = *material->uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = uboStride;

    auto uboWrite = vk::WriteDescriptorSet{};
    uboWrite.dstSet = *material->descriptorSet;
    uboWrite.dstBinding = 0;
    uboWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboWrite.descriptorCount = 1;
    uboWrite.pBufferInfo = &bufferInfo;

    // Tidy...
    auto imageView = *emptyImage_.view;
    if (data.diffuseTexture)
    {
        imageView = *resources_.images.get(data.diffuseTexture.value())->view;
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

    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    mesh->vertexBuffer = gpuDevice_.createVertexBuffer(vertices.size_bytes());
    mesh->vertexBufferMemory = gpuDevice_.allocateDeviceBufferMemory(mesh->vertexBuffer);
    mesh->indexBuffer = gpuDevice_.createIndexBuffer(indices.size_bytes());
    mesh->indexBufferMemory = gpuDevice_.allocateDeviceBufferMemory(mesh->indexBuffer);
    mesh->indexCount = static_cast<uint32_t>(indices.size());

    auto vertexStagingBuffer = gpuDevice_.createStagingBuffer(vertices.size_bytes());
    auto vertexStagingBufferMemory = gpuDevice_.allocateStagingBufferMemory(vertexStagingBuffer);
    auto indexStagingBuffer = gpuDevice_.createStagingBuffer(indices.size_bytes());
    auto indexStagingBufferMemory = gpuDevice_.allocateStagingBufferMemory(indexStagingBuffer);

    void* vertexStagingMemory = vertexStagingBufferMemory.mapMemory(0, VK_WHOLE_SIZE);
    void* indexStagingMemory = indexStagingBufferMemory.mapMemory(0, VK_WHOLE_SIZE);

    std::memcpy(vertexStagingMemory, vertices.data(), vertices.size_bytes());
    std::memcpy(indexStagingMemory, indices.data(), indices.size_bytes());

    vertexStagingBufferMemory.unmapMemory();
    indexStagingBufferMemory.unmapMemory();

    gpuDevice_.copyBuffer(cmd, vertexStagingBuffer, mesh->vertexBuffer, vertices.size_bytes());
    gpuDevice_.copyBuffer(cmd, indexStagingBuffer, mesh->indexBuffer, indices.size_bytes());

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    return handle;
}

SkyboxHandle Renderer::addSkybox(const std::array<FaceData, 6>& data)
{
    auto handle = resources_.skyboxes.allocate();
    auto skybox = resources_.skyboxes.get(handle);

    auto commandBuffers = gpuDevice_.createCommandBuffers(commandPool_, 1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    const auto width = data.at(0).width;
    const auto height = data.at(0).height;
    const auto imageSize = data.at(0).data.size_bytes();

    skybox->image = gpuDevice_.createCubemapImage(width, height);
    skybox->memory = gpuDevice_.allocateImageMemory(skybox->image, vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto stagingBuffer = gpuDevice_.createStagingBuffer(imageSize * 6);
    auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

    transitionImageLayout(*skybox->image,
                          *cmd,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageAspectFlagBits::eColor,
                          6);

    void* mappedMemory = stagingMemory.mapMemory(0, VK_WHOLE_SIZE);
    for (auto face = size_t{0}; face < 6; ++face)
    {
        auto dst = static_cast<uint8_t*>(mappedMemory) + (face * imageSize);
        std::memcpy(dst, data.at(face).data.data(), imageSize);
    }
    stagingMemory.unmapMemory();

    gpuDevice_.copyBufferToImage(cmd, stagingBuffer, skybox->image, width, height, 6);

    transitionImageLayout(*skybox->image,
                          *cmd,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageAspectFlagBits::eColor,
                          6);

    skybox->view = gpuDevice_.createCubemapImageView(skybox->image);

    cmd.end();
    gpuDevice_.submitCommandBuffer(cmd);

    skybox->descriptorSet = std::move(skyboxDescriptor_.allocateSets(1)[0]);

    auto imageInfo = vk::DescriptorImageInfo{};
    imageInfo.imageView = skybox->view;
    imageInfo.sampler = imageSampler_;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    auto textureWrite = vk::WriteDescriptorSet{};
    textureWrite.dstSet = skybox->descriptorSet;
    textureWrite.dstBinding = 0;
    textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    textureWrite.descriptorCount = 1;
    textureWrite.pImageInfo = &imageInfo;

    std::array writes{textureWrite};
    gpuDevice_.device().updateDescriptorSets(writes, {});

    return handle;
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
        auto buffer = gpuDevice_.createUniformBuffer(sizeof(CameraBufferObject));
        auto memory = gpuDevice_.allocateStagingBufferMemory(buffer);
        auto mappedMemory = memory.mapMemory(0, VK_WHOLE_SIZE);
        auto set = std::move(cameraDescriptor_.allocateSets(1)[0]);

        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = buffer;
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
        cameraUniformBuffersMemory_.push_back(std::move(memory));
        cameraUniformBuffersMappedMemory_.push_back(mappedMemory);
        cameraDescriptorSets_.push_back(std::move(set));
    }
}

void Renderer::createDirectionalLightBuffers()
{
    directionalLightUniformBuffer_ = gpuDevice_.createUniformBuffer(sizeof(DirectionalLightUboData));
    directionalLightUniformBufferMemory_ = gpuDevice_.allocateStagingBufferMemory(directionalLightUniformBuffer_);
    directionalLightUniformBufferMappedMemory_ = directionalLightUniformBufferMemory_.mapMemory(0, VK_WHOLE_SIZE);
    directionalLightDescriptorSet_ = std::move(directionalLightDescriptor_.allocateSets(1)[0]);

    auto bufferInfo = vk::DescriptorBufferInfo{};
    bufferInfo.buffer = directionalLightUniformBuffer_;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    auto dirLightUboWrite = vk::WriteDescriptorSet{};
    dirLightUboWrite.dstSet = directionalLightDescriptorSet_;
    dirLightUboWrite.dstBinding = 0;
    dirLightUboWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    dirLightUboWrite.descriptorCount = 1;
    dirLightUboWrite.pBufferInfo = &bufferInfo;

    auto dirLightWrite = std::array{dirLightUboWrite};
    gpuDevice_.device().updateDescriptorSets(dirLightWrite, {});
}

void Renderer::createShadowMapDescriptorSets()
{
    shadowMapDescriptorSet_ = std::move(shadowMapImageDescriptor_.allocateSets(1)[0]);

    auto imageInfo = vk::DescriptorImageInfo{};
    imageInfo.imageView = *shadowMapImageView_;
    imageInfo.sampler = *imageSampler_;
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
    pd.depthBiasConstantFactor = 0.0f;
    pd.depthBiasSlopeFactor = 0.0f;
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
                            /**shadowMapImageDescriptor_.layout()*/};
    pd.pushConstantRanges = {
        vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(GeometryPassPushConstants)}};
    pd.colorAttachmentFormats = {swapchain_.imageFormat()};
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
    pd.colorAttachmentFormats = {swapchain_.imageFormat()};
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
    pd.colorAttachmentFormats = {swapchain_.imageFormat()};
    pd.depthTestEnable = vk::False;
    pd.depthWriteEnable = vk::False;
    pd.depthCompareOp = vk::CompareOp::eNever;

    loadingScreenPass_ = createPipeline(gpuDevice_.device(), gpuDevice_.physicalDevice(), pd);
}

void Renderer::resizeGeometryPass()
{
    depthTargetImage_ = gpuDevice_.createDepthImage(windowExtent_.width, windowExtent_.height);
    depthTargetImageMemory_ = gpuDevice_.allocateImageMemory(depthTargetImage_,
                                                             vk::MemoryPropertyFlagBits::eDeviceLocal);
    depthTargetImageView_ = gpuDevice_.createDepthImageView(depthTargetImage_);
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

        resizeGeometryPass();
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
} // namespace renderer
