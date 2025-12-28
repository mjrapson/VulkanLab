#include "renderer/renderer.h"

#include "private/buffer.h"
#include "private/memory.h"
#include "private/shader.h"
#include "renderer/gpu_device.h"
#include "renderer/vertex.h"

#include <core/file_system.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <ranges>

namespace renderer
{
constexpr auto maxFramesInFlight = 2;

const std::vector<renderer::Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                                {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                                {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                                {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

vk::Extent2D getSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities,
                                int windowWidth,
                                int windowHeight)
{
    if (capabilities.currentExtent.width != 0xFFFFFFFF)
    {
        return capabilities.currentExtent;
    }

    return {std::clamp<uint32_t>(
                windowWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(windowHeight,
                                 capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height)};
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
    return format.format == vk::Format::eB8G8R8A8Srgb
           && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
}

vk::SurfaceFormatKHR getSurfaceFormat(const vk::PhysicalDevice& device,
                                      const vk::SurfaceKHR& surface)
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

void transitionImageLayout(const vk::Image& image,
                           const vk::raii::CommandBuffer& commandBuffer,
                           vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           vk::AccessFlags2 srcAccessMask,
                           vk::AccessFlags2 dstAccessMask,
                           vk::PipelineStageFlags2 srcStageMask,
                           vk::PipelineStageFlags2 dstStageMask)
{
    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto barrier = vk::ImageMemoryBarrier2{};
    barrier.srcStageMask = srcStageMask;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstStageMask = dstStageMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = subresourceRange;

    auto dependencyInfo = vk::DependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    commandBuffer.pipelineBarrier2(dependencyInfo);
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

    spdlog::info("Creating descriptor set layout");
    createDescriptorSetLayout();

    spdlog::info("Creating graphics pipeline");
    createGraphicsPipeline();

    spdlog::info("Creating command pool");
    createCommandPool();

    spdlog::info("Creating vertex buffers");
    createVertexBuffer();

    spdlog::info("Creating index buffer");
    createIndexBuffer();

    spdlog::info("Creating uniform buffers");
    createUniformBuffers();

    spdlog::info("Creating descriptor pool");
    createDescriptorPool();

    spdlog::info("Creating descriptor sets");
    createDescriptorSets();

    spdlog::info("Creating command buffers");
    createCommandBuffers();

    spdlog::info("Creating sync objects");
    createSyncObjects();
}

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
        std::tie(result, imageIndex) = swapchain_.acquireNextImage(
            UINT64_MAX, *presentCompleteSemaphores_.at(currentFrameIndex_), nullptr);
    }
    catch (const vk::OutOfDateKHRError&)
    {
        recreateSwapchain();
        return;
    }

    auto& commandBuffer = commandBuffers_.at(currentFrameIndex_);
    commandBuffer.reset();

    recordCommands(imageIndex, commandBuffer);

    updateUniformBuffer(currentFrameIndex_);

    const auto waitDestinationStageMask =
        vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);

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

void Renderer::createSwapchain()
{
    const auto surfaceCapabilities =
        gpuDevice_.physicalDevice().getSurfaceCapabilitiesKHR(*surface_);
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

void Renderer::createDescriptorSetLayout()
{
    auto uboLayoutBinding = vk::DescriptorSetLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    auto layoutInfo = vk::DescriptorSetLayoutCreateInfo{};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    descriptorSetLayout_ = vk::raii::DescriptorSetLayout(gpuDevice_.device(), layoutInfo);
}

void Renderer::createGraphicsPipeline()
{
    // Shader-progammable stages
    auto vertexShaderModule = createShaderModule(
        gpuDevice_.device(), core::readBinaryFile(core::getShaderDir() / "basic.vert.spv"));

    auto fragmentShaderModule = createShaderModule(
        gpuDevice_.device(), core::readBinaryFile(core::getShaderDir() / "basic.frag.spv"));

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
    const auto bindingDescriptions = Vertex::bindingDescription();
    const auto attributeDescriptions = Vertex::attributeDescriptions();
    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptions;
    vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    const auto dynamicStates =
        std::vector<vk::DynamicState>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

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
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
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

void Renderer::createCommandPool()
{
    auto poolInfo = vk::CommandPoolCreateInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = gpuDevice_.graphicsQueueFamilyIndex();

    commandPool_ = vk::raii::CommandPool(gpuDevice_.device(), poolInfo);
}

void Renderer::createVertexBuffer()
{
    const auto bufferSize = sizeof(Vertex) * vertices.size();

    auto stagingBuffer = createBuffer(gpuDevice_.device(),
                                      bufferSize,
                                      vk::BufferUsageFlagBits::eTransferSrc,
                                      vk::SharingMode::eExclusive);

    auto stagingBufferMemory = allocateBufferMemory(
        gpuDevice_.device(),
        gpuDevice_.physicalDevice(),
        stagingBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(dataStaging, vertices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();

    vertexBuffer_ =
        createBuffer(gpuDevice_.device(),
                     bufferSize,
                     vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                     vk::SharingMode::eExclusive);

    vertexBufferMemory_ = allocateBufferMemory(gpuDevice_.device(),
                                               gpuDevice_.physicalDevice(),
                                               vertexBuffer_,
                                               vk::MemoryPropertyFlagBits::eDeviceLocal);

    copyBuffer(gpuDevice_.device(),
               stagingBuffer,
               vertexBuffer_,
               gpuDevice_.graphicsQueue(),
               commandPool_,
               bufferSize);
}

void Renderer::createIndexBuffer()
{
    const auto bufferSize = sizeof(uint16_t) * indices.size();

    auto stagingBuffer = createBuffer(gpuDevice_.device(),
                                      bufferSize,
                                      vk::BufferUsageFlagBits::eTransferSrc,
                                      vk::SharingMode::eExclusive);

    auto stagingBufferMemory = allocateBufferMemory(
        gpuDevice_.device(),
        gpuDevice_.physicalDevice(),
        stagingBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(dataStaging, indices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();

    indexBuffer_ =
        createBuffer(gpuDevice_.device(),
                     bufferSize,
                     vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                     vk::SharingMode::eExclusive);

    indexBufferMemory_ = allocateBufferMemory(gpuDevice_.device(),
                                              gpuDevice_.physicalDevice(),
                                              indexBuffer_,
                                              vk::MemoryPropertyFlagBits::eDeviceLocal);

    copyBuffer(gpuDevice_.device(),
               stagingBuffer,
               indexBuffer_,
               gpuDevice_.graphicsQueue(),
               commandPool_,
               bufferSize);
}

void Renderer::createUniformBuffers()
{
    uniformBuffers_.clear();
    uniformBuffersMemory_.clear();
    mappedUniformBuffers_.clear();

    for ([[maybe_unused]] auto _ : std::views::repeat(0, maxFramesInFlight))
    {
        const auto bufferSize = sizeof(UniformBufferObject);
        auto buffer = createBuffer(gpuDevice_.device(),
                                   bufferSize,
                                   vk::BufferUsageFlagBits::eUniformBuffer,
                                   vk::SharingMode::eExclusive);

        auto bufferMemory = allocateBufferMemory(gpuDevice_.device(),
                                                 gpuDevice_.physicalDevice(),
                                                 buffer,
                                                 vk::MemoryPropertyFlagBits::eHostVisible
                                                     | vk::MemoryPropertyFlagBits::eHostCoherent);

        auto mappedMempory = bufferMemory.mapMemory(0, bufferSize);

        uniformBuffers_.emplace_back(std::move(buffer));
        uniformBuffersMemory_.emplace_back(std::move(bufferMemory));
        mappedUniformBuffers_.emplace_back(std::move(mappedMempory));
    }
}

void Renderer::createDescriptorPool()
{
    auto poolSize = vk::DescriptorPoolSize{};
    poolSize.type = vk::DescriptorType::eUniformBuffer;
    poolSize.descriptorCount = maxFramesInFlight;

    auto poolInfo = vk::DescriptorPoolCreateInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = maxFramesInFlight;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    descriptorPool_ = vk::raii::DescriptorPool(gpuDevice_.device(), poolInfo);
}

void Renderer::createDescriptorSets()
{
    descriptorSets_.clear();

    auto layouts = std::vector<vk::DescriptorSetLayout>(maxFramesInFlight, *descriptorSetLayout_);

    auto allocInfo = vk::DescriptorSetAllocateInfo{};
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets_ = gpuDevice_.device().allocateDescriptorSets(allocInfo);

    for ([[maybe_unused]] auto frameIndex : std::views::repeat(0, maxFramesInFlight))
    {
        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = uniformBuffers_.at(frameIndex);
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        auto writeInfo = vk::WriteDescriptorSet{};
        writeInfo.dstSet = descriptorSets_.at(frameIndex);
        writeInfo.dstBinding = 0;
        writeInfo.dstArrayElement = 0;
        writeInfo.descriptorCount = 1;
        writeInfo.descriptorType = vk::DescriptorType::eUniformBuffer;
        writeInfo.pBufferInfo = &bufferInfo;

        gpuDevice_.device().updateDescriptorSets(writeInfo, {});
    }
}

void Renderer::createCommandBuffers()
{
    auto allocInfo = vk::CommandBufferAllocateInfo{};
    allocInfo.commandPool = *commandPool_;
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
    commandBuffer.bindVertexBuffers(0, *vertexBuffer_, {0});
    commandBuffer.bindIndexBuffer(*indexBuffer_, 0, vk::IndexType::eUint16);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     pipelineLayout_,
                                     0,
                                     *descriptorSets_.at(currentFrameIndex_),
                                     nullptr);

    commandBuffer.setViewport(0,
                              vk::Viewport(0.0f,
                                           0.0f,
                                           static_cast<float>(swapchainExtent_.width),
                                           static_cast<float>(swapchainExtent_.height),
                                           0.0f,
                                           1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent_));

    commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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

void Renderer::updateUniformBuffer(uint32_t frameIndex)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    const auto currentTime = std::chrono::high_resolution_clock::now();
    const auto time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    auto ubo = UniformBufferObject{};
    ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = lookAt(
        glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.projection = glm::perspective(glm::radians(45.0f),
                                      static_cast<float>(swapchainExtent_.width)
                                          / static_cast<float>(swapchainExtent_.height),
                                      0.1f,
                                      10.0f);
    ubo.projection[1][1] *= -1;
    memcpy(mappedUniformBuffers_[frameIndex], &ubo, sizeof(ubo));
}
} // namespace renderer