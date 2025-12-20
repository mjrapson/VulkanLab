#include "renderer/renderer.h"

#include "private/buffer.h"
#include "private/memory.h"
#include "renderer/gpu_device.h"
#include "renderer/vertex.h"

#include <core/file_system.h>

#include <spdlog/spdlog.h>

#include <ranges>

namespace renderer
{
constexpr auto maxFramesInFlight = 2;

const auto vertices = std::vector<renderer::Vertex>{{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                                    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                                    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

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

[[nodiscard]] vk::raii::ShaderModule createShaderModule(const vk::raii::Device& device,
                                                        const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                          .pCode = reinterpret_cast<const uint32_t*>(code.data())};

    vk::raii::ShaderModule shaderModule{device, createInfo};
    return shaderModule;
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
    const auto barrier =
        vk::ImageMemoryBarrier2{.srcStageMask = srcStageMask,
                                .srcAccessMask = srcAccessMask,
                                .dstStageMask = dstStageMask,
                                .dstAccessMask = dstAccessMask,
                                .oldLayout = oldLayout,
                                .newLayout = newLayout,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image = image,
                                .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                     .baseMipLevel = 0,
                                                     .levelCount = 1,
                                                     .baseArrayLayer = 0,
                                                     .layerCount = 1}};
    const auto dependencyInfo = vk::DependencyInfo{
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

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

    spdlog::info("Creating graphics pipeline");
    createGraphicsPipeline();

    spdlog::info("Creating command pool");
    createCommandPool();

    spdlog::info("Creating vertex buffers");
    createVertexBuffer();

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

    const auto waitDestinationStageMask =
        vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);

    const auto submitInfo =
        vk::SubmitInfo{.waitSemaphoreCount = 1,
                       .pWaitSemaphores = &*presentCompleteSemaphores_.at(currentFrameIndex_),
                       .pWaitDstStageMask = &waitDestinationStageMask,
                       .commandBufferCount = 1,
                       .pCommandBuffers = &*commandBuffer,
                       .signalSemaphoreCount = 1,
                       .pSignalSemaphores = &*renderFinishedSemaphores_.at(imageIndex)};

    gpuDevice_.device().resetFences(*drawFences_.at(currentFrameIndex_));
    gpuDevice_.graphicsQueue().submit(submitInfo, *drawFences_.at(currentFrameIndex_));

    const auto presentInfoKHR =
        vk::PresentInfoKHR{.waitSemaphoreCount = 1,
                           .pWaitSemaphores = &*renderFinishedSemaphores_.at(imageIndex),
                           .swapchainCount = 1,
                           .pSwapchains = &*swapchain_,
                           .pImageIndices = &imageIndex};

    try
    {
        result = gpuDevice_.presentQueue().presentKHR(presentInfoKHR);
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
    surfaceFormat_ = getSurfaceFormat(gpuDevice_.physicalDevice(), surface_);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .surface = *surface_,
        .minImageCount = getSurfaceMinImageCount(surfaceCapabilities),
        .imageFormat = surfaceFormat_.format,
        .imageColorSpace = surfaceFormat_.colorSpace,
        .imageExtent = swapchainExtent_,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = true};

    swapchain_ = vk::raii::SwapchainKHR(gpuDevice_.device(), swapChainCreateInfo);
    swapchainImages_ = swapchain_.getImages();
}

void Renderer::createSwapchainImageViews()
{
    swapchainImageViews_.clear();

    auto imageViewCreateInfo =
        vk::ImageViewCreateInfo{.viewType = vk::ImageViewType::e2D,
                                .format = surfaceFormat_.format,
                                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

    for (const auto& image : swapchainImages_)
    {
        imageViewCreateInfo.image = image;
        swapchainImageViews_.emplace_back(gpuDevice_.device(), imageViewCreateInfo);
    }
}

void Renderer::createGraphicsPipeline()
{
    // Shader-progammable stages
    auto vertexShaderModule = createShaderModule(
        gpuDevice_.device(), core::readBinaryFile(core::getShaderDir() / "basic.vert.spv"));

    auto fragmentShaderModule = createShaderModule(
        gpuDevice_.device(), core::readBinaryFile(core::getShaderDir() / "basic.frag.spv"));

    const auto vertShaderStageInfo =
        vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eVertex,
                                          .module = vertexShaderModule,
                                          .pName = "vertMain"};

    const auto fragShaderStageInfo =
        vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eFragment,
                                          .module = fragmentShaderModule,
                                          .pName = "fragMain"};

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Fixed function stages
    const auto bindingDescriptions = Vertex::bindingDescription();
    const auto attributeDescriptions = Vertex::attributeDescriptions();
    const auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescriptions,
        .vertexAttributeDescriptionCount = attributeDescriptions.size(),
        .pVertexAttributeDescriptions = attributeDescriptions.data()};

    const auto inputAssembly =
        vk::PipelineInputAssemblyStateCreateInfo{.topology = vk::PrimitiveTopology::eTriangleList};

    const auto dynamicStates =
        std::vector<vk::DynamicState>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    const auto dynamicState = vk::PipelineDynamicStateCreateInfo{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    const auto viewportState =
        vk::PipelineViewportStateCreateInfo{.viewportCount = 1, .scissorCount = 1};

    const auto rasterizer =
        vk::PipelineRasterizationStateCreateInfo{.depthClampEnable = vk::False,
                                                 .rasterizerDiscardEnable = vk::False,
                                                 .polygonMode = vk::PolygonMode::eFill,
                                                 .cullMode = vk::CullModeFlagBits::eBack,
                                                 .frontFace = vk::FrontFace::eClockwise,
                                                 .depthBiasEnable = vk::False,
                                                 .depthBiasSlopeFactor = 1.0f,
                                                 .lineWidth = 1.0f};

    const auto multisampling = vk::PipelineMultisampleStateCreateInfo{
        .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

    const auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{
        .blendEnable = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                          | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

    const auto colorBlending =
        vk::PipelineColorBlendStateCreateInfo{.logicOpEnable = vk::False,
                                              .logicOp = vk::LogicOp::eCopy,
                                              .attachmentCount = 1,
                                              .pAttachments = &colorBlendAttachment};

    const auto pipelineLayoutInfo =
        vk::PipelineLayoutCreateInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};

    pipelineLayout_ = vk::raii::PipelineLayout(gpuDevice_.device(), pipelineLayoutInfo);

    // Render passes (dynamic rendering)
    const auto pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{
        .colorAttachmentCount = 1, .pColorAttachmentFormats = &surfaceFormat_.format};

    const auto pipelineInfo = vk::GraphicsPipelineCreateInfo{.pNext = &pipelineRenderingCreateInfo,
                                                             .stageCount = 2,
                                                             .pStages = shaderStages,
                                                             .pVertexInputState = &vertexInputInfo,
                                                             .pInputAssemblyState = &inputAssembly,
                                                             .pViewportState = &viewportState,
                                                             .pRasterizationState = &rasterizer,
                                                             .pMultisampleState = &multisampling,
                                                             .pColorBlendState = &colorBlending,
                                                             .pDynamicState = &dynamicState,
                                                             .layout = pipelineLayout_,
                                                             .renderPass = nullptr};

    graphicsPipeline_ = vk::raii::Pipeline(gpuDevice_.device(), nullptr, pipelineInfo);
}

void Renderer::createCommandPool()
{
    const auto poolInfo =
        vk::CommandPoolCreateInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                  .queueFamilyIndex = gpuDevice_.graphicsQueueFamilyIndex()};

    commandPool_ = vk::raii::CommandPool(gpuDevice_.device(), poolInfo);
}

void Renderer::createVertexBuffer()
{
    const auto bufferSize = sizeof(Vertex) * vertices.size();

    vertexBuffer_ = createBuffer(gpuDevice_.device(),
                                 bufferSize,
                                 vk::BufferUsageFlagBits::eVertexBuffer,
                                 vk::SharingMode::eExclusive);

    vertexBufferMemory_ = allocateBufferMemory(gpuDevice_.device(),
                                               gpuDevice_.physicalDevice(),
                                               vertexBuffer_,
                                               vk::MemoryPropertyFlagBits::eHostVisible
                                                   | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = vertexBufferMemory_.mapMemory(0, bufferSize);
    memcpy(data, vertices.data(), bufferSize);
    vertexBufferMemory_.unmapMemory();
}

void Renderer::createCommandBuffers()
{
    const auto allocInfo = vk::CommandBufferAllocateInfo{.commandPool = commandPool_,
                                                         .level = vk::CommandBufferLevel::ePrimary,
                                                         .commandBufferCount = maxFramesInFlight};

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
        drawFences_.emplace_back(gpuDevice_.device(),
                                 vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
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
    const auto attachmentInfo =
        vk::RenderingAttachmentInfo{.imageView = swapchainImageViews_.at(imageIndex),
                                    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                    .loadOp = vk::AttachmentLoadOp::eClear,
                                    .storeOp = vk::AttachmentStoreOp::eStore,
                                    .clearValue = clearColor};

    const auto renderingInfo =
        vk::RenderingInfo{.renderArea = {.offset = {0, 0}, .extent = swapchainExtent_},
                          .layerCount = 1,
                          .colorAttachmentCount = 1,
                          .pColorAttachments = &attachmentInfo};

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline_);
    commandBuffer.bindVertexBuffers(0, *vertexBuffer_, {0});

    commandBuffer.setViewport(0,
                              vk::Viewport(0.0f,
                                           0.0f,
                                           static_cast<float>(swapchainExtent_.width),
                                           static_cast<float>(swapchainExtent_.height),
                                           0.0f,
                                           1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent_));

    commandBuffer.draw(vertices.size(), 1, 0, 0);

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
} // namespace renderer