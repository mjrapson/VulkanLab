#include "renderer/renderer.h"

#include "private/buffer.h"
#include "private/gpu_material.h"
#include "private/gpu_resource_cache.h"
#include "private/image.h"
#include "private/memory.h"
#include "private/shader.h"
#include "renderer/gpu_device.h"
#include "renderer/vertex_layout.h"

#include <assets/asset_database.h>
#include <core/file_system.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <ranges>

namespace renderer
{
struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

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

    spdlog::info("Creating graphics pipeline");
    createDescriptorPool();
    createDescriptorSetLayout();
    createGraphicsPipeline();

    spdlog::info("Creating command buffers");
    createCommandBuffers();

    spdlog::info("Creating sync objects");
    createSyncObjects();

    spdlog::info("Creating default objects");
    createDefaultObjects();
}

Renderer::~Renderer() = default;

void Renderer::renderFrame()
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

    recordCommands(imageIndex, commandBuffer);

    // updateUniformBuffer(currentFrameIndex_);

    const auto waitDestinationStageMask = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);

    auto submitInfo = vk::SubmitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*presentCompleteSemaphores_.at(currentFrameIndex_);
    submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &*renderFinishedSemaphores_.at(imageIndex);

    gpuDevice_.device().resetFences(*drawFences_.at(currentFrameIndex_));
    gpuDevice_.graphicsQueue().submit(submitInfo, *drawFences_.at(currentFrameIndex_));

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

    currentFrameIndex_ = (currentFrameIndex_ + 1) & gpuDevice_.maxFramesInFlight();
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

    // To move into a pipeline object
    auto layouts = std::vector<vk::DescriptorSetLayout>{gpuDevice_.maxFramesInFlight(), *descriptorSetLayout_};

    auto stride = alignMemory(sizeof(GpuMaterialBufferData),
                              gpuDevice_.physicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);

    for (const auto& [handle, material] : db.materials().entries())
    {
        auto allocInfo = vk::DescriptorSetAllocateInfo{};
        allocInfo.descriptorPool = *descriptorPool_;
        allocInfo.descriptorSetCount = gpuDevice_.maxFramesInFlight();
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets_[handle] = std::move(vk::raii::DescriptorSets{gpuDevice_.device(), allocInfo});

        for (auto frameIndex = uint32_t{0}; frameIndex < gpuDevice_.maxFramesInFlight(); ++frameIndex)
        {
            auto bufferInfo = vk::DescriptorBufferInfo{};
            bufferInfo.buffer = gpuResources_->materialUniformBuffer(frameIndex);
            bufferInfo.offset = 0;
            bufferInfo.range = stride;

            auto uboWrite = vk::WriteDescriptorSet{};
            uboWrite.dstSet = descriptorSets_.at(handle).at(frameIndex);
            uboWrite.dstBinding = 0;
            uboWrite.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            uboWrite.descriptorCount = 1;
            uboWrite.pBufferInfo = &bufferInfo;

            vk::DescriptorImageInfo imageInfo{};
            imageInfo.imageView = material.diffuseTexture
                                      ? gpuResources_->gpuImage(material.diffuseTexture.value()).view
                                      : emptyImageView;
            imageInfo.sampler = material.diffuseTexture
                                    ? gpuResources_->gpuImage(material.diffuseTexture.value()).sampler
                                    : emptyImageSampler;
            ;
            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::WriteDescriptorSet textureWrite{};
            textureWrite.dstSet = descriptorSets_.at(handle).at(frameIndex);
            textureWrite.dstBinding = 1;
            textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            textureWrite.descriptorCount = 1;
            textureWrite.pImageInfo = &imageInfo;

            std::array writes{uboWrite, textureWrite};
            gpuDevice_.device().updateDescriptorSets(writes, {});
        }
    }
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

// To move into a pipeline object
void Renderer::createDescriptorPool()
{
    auto materialUboPoolSize = vk::DescriptorPoolSize{};
    materialUboPoolSize.type = vk::DescriptorType::eUniformBufferDynamic;
    materialUboPoolSize.descriptorCount = gpuDevice_.maxFramesInFlight();

    auto texturePoolSize = vk::DescriptorPoolSize{};
    texturePoolSize.type = vk::DescriptorType::eCombinedImageSampler;
    texturePoolSize.descriptorCount = gpuDevice_.maxFramesInFlight();

    auto poolSizes = std::array{materialUboPoolSize, texturePoolSize};

    auto poolInfo = vk::DescriptorPoolCreateInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = gpuDevice_.maxFramesInFlight();
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    descriptorPool_ = vk::raii::DescriptorPool{gpuDevice_.device(), poolInfo};
}

// To move into a pipeline object
void Renderer::createDescriptorSetLayout()
{
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

    auto bindings = std::array{materialUboLayoutBinding, textureBinding};

    auto layoutInfo = vk::DescriptorSetLayoutCreateInfo{};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    descriptorSetLayout_ = vk::raii::DescriptorSetLayout(gpuDevice_.device(), layoutInfo);
}

// To move into a pipeline object
void Renderer::createGraphicsPipeline()
{
    // Shader-progammable stages
    auto vertexShaderModule = createShaderModule(gpuDevice_.device(),
                                                 core::readBinaryFile(core::getShaderDir() / "basic.vert.spv"));

    auto fragmentShaderModule = createShaderModule(gpuDevice_.device(),
                                                   core::readBinaryFile(core::getShaderDir() / "basic.frag.spv"));

    auto vertShaderStageInfo = vk::PipelineShaderStageCreateInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = *vertexShaderModule;
    vertShaderStageInfo.pName = "vertMain";

    auto fragShaderStageInfo = vk::PipelineShaderStageCreateInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = *fragmentShaderModule;
    fragShaderStageInfo.pName = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Fixed function stages
    const auto bindingDescriptions = VertexLayout::bindingDescription();
    const auto attributeDescriptions = VertexLayout::attributeDescriptions();
    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptions;
    vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    const auto dynamicStates = std::vector<vk::DynamicState>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    auto dynamicState = vk::PipelineDynamicStateCreateInfo{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    auto viewportState = vk::PipelineViewportStateCreateInfo{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    auto rasterizer = vk::PipelineRasterizationStateCreateInfo{};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.depthBiasSlopeFactor = 1.0f;
    rasterizer.lineWidth = 1.0;

    auto multisampling = vk::PipelineMultisampleStateCreateInfo{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{};
    colorBlendAttachment.blendEnable = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                          | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    auto colorBlending = vk::PipelineColorBlendStateCreateInfo{};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    pipelineLayout_ = vk::raii::PipelineLayout(gpuDevice_.device(), pipelineLayoutInfo);

    // Render passes (dynamic rendering)
    auto pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &surfaceFormat_.format;

    auto pipelineInfo = vk::GraphicsPipelineCreateInfo{};
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = *pipelineLayout_;
    pipelineInfo.renderPass = nullptr;

    graphicsPipeline_ = vk::raii::Pipeline(gpuDevice_.device(), nullptr, pipelineInfo);
}

void Renderer::createCommandBuffers()
{
    auto allocInfo = vk::CommandBufferAllocateInfo{};
    allocInfo.commandPool = *gpuDevice_.commandPool();
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = gpuDevice_.maxFramesInFlight();

    commandBuffers_ = vk::raii::CommandBuffers(gpuDevice_.device(), allocInfo);
}

void Renderer::createSyncObjects()
{
    for ([[maybe_unused]] auto _ : std::views::repeat(0, swapchainImages_.size()))
    {
        renderFinishedSemaphores_.emplace_back(gpuDevice_.device(), vk::SemaphoreCreateInfo{});
    }

    for ([[maybe_unused]] auto _ : std::views::repeat(0, gpuDevice_.maxFramesInFlight()))
    {
        presentCompleteSemaphores_.emplace_back(gpuDevice_.device(), vk::SemaphoreCreateInfo{});

        auto fenceCreateInfo = vk::FenceCreateInfo{};
        fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;
        drawFences_.emplace_back(gpuDevice_.device(), fenceCreateInfo);
    }
}

void Renderer::createDefaultObjects()
{
    emptyImage = createImage(gpuDevice_.device(), 1, 1);
    emptyImageMemory = allocateImageMemory(gpuDevice_.device(),
                                           gpuDevice_.physicalDevice(),
                                           emptyImage,
                                           vk::MemoryPropertyFlagBits::eDeviceLocal);

    const auto imageSize = 4; //  RGBA8
    auto stagingBuffer = createBuffer(gpuDevice_.device(),
                                      imageSize,
                                      vk::BufferUsageFlagBits::eTransferSrc,
                                      vk::SharingMode::eExclusive);

    auto stagingMemory = allocateBufferMemory(gpuDevice_.device(),
                                              gpuDevice_.physicalDevice(),
                                              stagingBuffer,
                                              vk::MemoryPropertyFlagBits::eHostVisible
                                                  | vk::MemoryPropertyFlagBits::eHostCoherent);

    auto imageData = std::vector<std::byte>{std::byte{1}, std::byte{1}, std::byte{1}, std::byte{1}};
    void* data = stagingMemory.mapMemory(0, imageSize);
    std::memcpy(data, imageData.data(), imageSize);
    stagingMemory.unmapMemory();

    copyBufferToImage(gpuDevice_.device(),
                      stagingBuffer,
                      emptyImage,
                      gpuDevice_.graphicsQueue(),
                      gpuDevice_.commandPool(),
                      1,
                      1);

    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.image = *emptyImage;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = vk::Format::eR8G8B8A8Srgb;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    emptyImageView = vk::raii::ImageView{gpuDevice_.device(), imageViewCreateInfo};

    auto samplerInfo = vk::SamplerCreateInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear, samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    emptyImageSampler = vk::raii::Sampler{gpuDevice_.device(), samplerInfo};
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
}

void Renderer::recordCommands(uint32_t imageIndex, const vk::raii::CommandBuffer& commandBuffer)
{
    commandBuffer.begin({});

    transitionImageLayout(swapchainImages_.at(imageIndex),
                          commandBuffer,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          {}, // srcAccessMask (no need to wait for previous operations)
                          vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput  // dstStage
    );

    const auto clearColor = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};

    auto attachmentInfo = vk::RenderingAttachmentInfo{};
    attachmentInfo.imageView = *swapchainImageViews_.at(imageIndex);
    attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    attachmentInfo.clearValue = clearColor;

    auto renderingInfo = vk::RenderingInfo{};
    renderingInfo.renderArea = {.offset = {0, 0}, .extent = swapchainExtent_};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline_);
    // commandBuffer.bindVertexBuffers(0, *vertexBuffer_, {0});
    // commandBuffer.bindIndexBuffer(*indexBuffer_, 0, vk::IndexType::eUint16);
    // commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
    //                                  pipelineLayout_,
    //                                  0,
    //                                  *descriptorSets_.at(currentFrameIndex_),
    //                                  nullptr);

    commandBuffer.setViewport(0,
                              vk::Viewport(0.0f,
                                           0.0f,
                                           static_cast<float>(swapchainExtent_.width),
                                           static_cast<float>(swapchainExtent_.height),
                                           0.0f,
                                           1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent_));

    // commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    commandBuffer.endRendering();

    transitionImageLayout(swapchainImages_.at(imageIndex),
                          commandBuffer,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::ImageLayout::ePresentSrcKHR,
                          vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                          {},                                                 // dstAccessMask
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                          vk::PipelineStageFlagBits2::eBottomOfPipe           // dstStage
    );

    commandBuffer.end();
}

// void Renderer::updateUniformBuffer(uint32_t frameIndex)
// {
//     static auto startTime = std::chrono::high_resolution_clock::now();

//     const auto currentTime = std::chrono::high_resolution_clock::now();
//     const auto time =
//         std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

//     auto ubo = UniformBufferObject{};
//     ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//     ubo.view = lookAt(
//         glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//     ubo.projection = glm::perspective(glm::radians(45.0f),
//                                       static_cast<float>(swapchainExtent_.width)
//                                           / static_cast<float>(swapchainExtent_.height),
//                                       0.1f,
//                                       10.0f);
//     ubo.projection[1][1] *= -1;
//     memcpy(mappedUniformBuffers_[frameIndex], &ubo, sizeof(ubo));
// }
} // namespace renderer
