#include "renderer/renderer.h"

#include "private/buffer.h"
#include "private/gpu_material.h"
#include "private/gpu_resource_cache.h"
#include "private/image.h"
#include "private/memory.h"
#include "private/shader.h"
#include "renderer/camera.h"
#include "renderer/gpu_device.h"
#include "renderer/vertex_layout.h"

#include <assets/asset_database.h>
#include <core/file_system.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <chrono>
#include <ranges>

namespace renderer
{

constexpr auto maxFramesInFlight = 2;

// To move into a pipeline object
struct CameraBufferObject
{
    glm::mat4 view;
    glm::mat4 projection;
};

// To move into a pipeline object
struct PushConstants
{
    glm::mat4 modelTransform;
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
    createCameraDescriptorPool();
    createDescriptorSetLayouts();
    createGraphicsPipeline();
    createCameraBuffers();

    spdlog::info("Creating command buffers");
    createCommandBuffers();

    spdlog::info("Creating sync objects");
    createSyncObjects();

    createDefaultImage();
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
    // Hack (temp)
    auto materials = uint32_t{0};
    for (const auto& prefab : db.prefabs())
    {
        for (const auto& material : prefab.second->materials())
        {
            materials++;
        }
    }

    createMaterialDescriptorPools(materials);

    gpuResources_ = std::make_unique<GpuResourceCache>(db, gpuDevice_, maxFramesInFlight);

    // To move into a pipeline object
    auto layouts = std::vector<vk::DescriptorSetLayout>{maxFramesInFlight, *materialDescriptorSetLayout_};

    auto stride = alignMemory(sizeof(GpuMaterialBufferData),
                              gpuDevice_.physicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);

    for (const auto& prefab : db.prefabs())
    {
        for (const auto& material : prefab.second->materials())
        {
            auto allocInfo = vk::DescriptorSetAllocateInfo{};
            allocInfo.descriptorPool = *materialDescriptorPool_;
            allocInfo.descriptorSetCount = maxFramesInFlight;
            allocInfo.pSetLayouts = layouts.data();

            materialDescriptorSets_[material.second.get()] = std::move(
                vk::raii::DescriptorSets{gpuDevice_.device(), allocInfo});

            for (auto frameIndex = uint32_t{0}; frameIndex < maxFramesInFlight; ++frameIndex)
            {
                auto bufferInfo = vk::DescriptorBufferInfo{};
                bufferInfo.buffer = gpuResources_->materialUniformBuffer(frameIndex);
                bufferInfo.offset = 0;
                bufferInfo.range = stride;

                auto uboWrite = vk::WriteDescriptorSet{};
                uboWrite.dstSet = materialDescriptorSets_.at(material.second.get()).at(frameIndex);
                uboWrite.dstBinding = 0;
                uboWrite.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
                uboWrite.descriptorCount = 1;
                uboWrite.pBufferInfo = &bufferInfo;

                auto imageInfo = vk::DescriptorImageInfo{};
                if (material.second->diffuseTexture)
                {
                    imageInfo.imageView = gpuResources_->gpuImage(material.second->diffuseTexture).view;
                    imageInfo.sampler = gpuResources_->gpuImage(material.second->diffuseTexture).sampler;
                }
                else
                {
                    imageInfo.imageView = emptyImageView_;
                    imageInfo.sampler = emptyImageSampler_;
                }
                imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

                auto textureWrite = vk::WriteDescriptorSet{};
                textureWrite.dstSet = materialDescriptorSets_.at(material.second.get()).at(frameIndex);
                textureWrite.dstBinding = 1;
                textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                textureWrite.descriptorCount = 1;
                textureWrite.pImageInfo = &imageInfo;

                std::array writes{uboWrite, textureWrite};
                gpuDevice_.device().updateDescriptorSets(writes, {});
            }
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

// To move into a pipeline object
void Renderer::createMaterialDescriptorPools(uint32_t materialCount)
{
    auto materialUboPoolSize = vk::DescriptorPoolSize{};
    materialUboPoolSize.type = vk::DescriptorType::eUniformBufferDynamic;
    materialUboPoolSize.descriptorCount = maxFramesInFlight;

    auto texturePoolSize = vk::DescriptorPoolSize{};
    texturePoolSize.type = vk::DescriptorType::eCombinedImageSampler;
    texturePoolSize.descriptorCount = maxFramesInFlight;

    auto materialPoolSizes = std::array{materialUboPoolSize, texturePoolSize};

    auto materialPoolInfo = vk::DescriptorPoolCreateInfo{};
    materialPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    materialPoolInfo.maxSets = maxFramesInFlight * materialCount;
    materialPoolInfo.poolSizeCount = static_cast<uint32_t>(materialPoolSizes.size());
    materialPoolInfo.pPoolSizes = materialPoolSizes.data();

    materialDescriptorPool_ = vk::raii::DescriptorPool{gpuDevice_.device(), materialPoolInfo};
}

// To move into a pipeline object
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

    auto descriptorSetLayouts = std::array{*cameraDescriptorSetLayout_, *materialDescriptorSetLayout_};

    if (gpuDevice_.physicalDevice().getProperties().limits.maxPushConstantsSize < sizeof(PushConstants))
    {
        throw std::runtime_error{"Requested push constant size exceeds device limits"};
    }

    auto pushConstantRange = vk::PushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

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
    allocInfo.commandBufferCount = maxFramesInFlight;

    commandBuffers_ = vk::raii::CommandBuffers(gpuDevice_.device(), allocInfo);
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
        auto buffer = createBuffer(gpuDevice_.device(),
                                   sizeof(CameraBufferObject),
                                   vk::BufferUsageFlagBits::eUniformBuffer,
                                   vk::SharingMode::eExclusive);

        auto memory = allocateBufferMemory(gpuDevice_.device(),
                                           gpuDevice_.physicalDevice(),
                                           buffer,
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
}

void Renderer::recordCommands(uint32_t imageIndex,
                              const vk::raii::CommandBuffer& commandBuffer,
                              const renderer::Camera& camera,
                              const std::vector<DrawCommand>& drawCommands)
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
    commandBuffer.bindVertexBuffers(0, *gpuResources_->meshVertexBuffer(), {0});
    commandBuffer.bindIndexBuffer(*gpuResources_->meshIndexBuffer(), 0, vk::IndexType::eUint32);

    auto cameraBuffer = CameraBufferObject{};
    cameraBuffer.projection = camera.projection();
    cameraBuffer.view = camera.view();

    memcpy(cameraUboMappedMemory_[currentFrameIndex_], &cameraBuffer, sizeof(cameraBuffer));

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     pipelineLayout_,
                                     0,
                                     *cameraDescriptorSets_.at(currentFrameIndex_),
                                     nullptr);

    commandBuffer.setViewport(0,
                              vk::Viewport(0.0f,
                                           0.0f,
                                           static_cast<float>(swapchainExtent_.width),
                                           static_cast<float>(swapchainExtent_.height),
                                           0.0f,
                                           1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent_));

    for (const auto& drawCommand : drawCommands)
    {
        auto pushConstants = PushConstants{};
        pushConstants.modelTransform = drawCommand.transform;

        commandBuffer.pushConstants(pipelineLayout_,
                                    vk::ShaderStageFlagBits::eVertex,
                                    0,
                                    vk::ArrayProxy<const PushConstants>{pushConstants});

        if (drawCommand.mesh->material)
        {
            const auto& gpuMaterial = gpuResources_->gpuMaterial(drawCommand.mesh->material);
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                pipelineLayout_,
                1,
                *materialDescriptorSets_.at(drawCommand.mesh->material).at(currentFrameIndex_),
                gpuMaterial.uboOffset);
        }

        auto& gpuMesh = gpuResources_->gpuMesh(drawCommand.mesh);
        commandBuffer.drawIndexed(gpuMesh.indexCount, 1, gpuMesh.indexOffset, gpuMesh.vertexOffset, 0);
    }

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

void Renderer::createDefaultImage()
{
    emptyImage_ = createImage(gpuDevice_.device(), 1, 1);
    emptyImageMemory_ = allocateImageMemory(gpuDevice_.device(),
                                            gpuDevice_.physicalDevice(),
                                            emptyImage_,
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
                      emptyImage_,
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
    imageViewCreateInfo.image = *emptyImage_;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = vk::Format::eR8G8B8A8Srgb;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    emptyImageView_ = vk::raii::ImageView{gpuDevice_.device(), imageViewCreateInfo};

    auto samplerInfo = vk::SamplerCreateInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear, samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    emptyImageSampler_ = vk::raii::Sampler{gpuDevice_.device(), samplerInfo};
}
} // namespace renderer
