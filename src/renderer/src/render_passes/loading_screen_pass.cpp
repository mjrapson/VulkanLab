/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "loading_screen_pass.h"

#include "renderer/gpu_device.h"
#include "renderer/gpu_objects.h"

#include <core/file_system.h>

namespace renderer
{
constexpr auto imageDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
};

LoadingScreenPass::LoadingScreenPass(const GpuDevice& gpuDevice)
    : gpuDevice_{gpuDevice},
      imageDescriptor_{gpuDevice_, imageDescriptorBindings}
{
}

void LoadingScreenPass::initialize(const vk::Extent2D& extent,
                                   const vk::Format& surfaceFormat,
                                   uint32_t maxFramesInFlight)
{
    resize(extent);

    createPipeline(surfaceFormat);

    imageDescriptor_.resize(maxFramesInFlight);
}

void LoadingScreenPass::resize(const vk::Extent2D& extent)
{
    extent_ = extent;
}

void LoadingScreenPass::rebuild(std::unique_ptr<Image> loadingScreenImage)
{
    loadingScreenImage_ = std::move(loadingScreenImage);

    if (!loadingScreenImage_)
    {
        return;
    }

    const auto count = imageDescriptor_.size();

    imageDescriptorSets_ = imageDescriptor_.allocateSets(count);

    for (auto frameIndex = uint32_t{0}; frameIndex < count; ++frameIndex)
    {
        auto imageInfo = vk::DescriptorImageInfo{};
        imageInfo.imageView = loadingScreenImage_->view;
        imageInfo.sampler = loadingScreenImage_->sampler;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        auto imageWrite = vk::WriteDescriptorSet{};
        imageWrite.dstSet = *imageDescriptorSets_.at(frameIndex);
        imageWrite.dstBinding = 0;
        imageWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        imageWrite.descriptorCount = 1;
        imageWrite.pImageInfo = &imageInfo;

        auto writes = std::array{imageWrite};
        gpuDevice_.device().updateDescriptorSets(writes, {});
    }
}

void LoadingScreenPass::recordCommands(uint32_t frameIndex,
                                       const vk::raii::CommandBuffer& commandBuffer,
                                       vk::ImageView colorTargetImageView)
{
    if (!loadingScreenImage_)
    {
        return;
    }

    const auto clearColor = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};

    auto attachmentInfo = vk::RenderingAttachmentInfo{};
    attachmentInfo.imageView = colorTargetImageView;
    attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    attachmentInfo.clearValue = clearColor;

    auto renderingInfo = vk::RenderingInfo{};
    renderingInfo.renderArea = {.offset = {0, 0}, .extent = extent_};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     pipelineLayout_,
                                     0,
                                     *imageDescriptorSets_.at(frameIndex),
                                     nullptr);

    commandBuffer.setViewport(
        0,
        vk::Viewport(0.0f, 0.0f, static_cast<float>(extent_.width), static_cast<float>(extent_.height), 0.0f, 1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent_));

    commandBuffer.draw(6, 1, 0, 0);

    commandBuffer.endRendering();
}

void LoadingScreenPass::createPipeline(const vk::Format& surfaceFormat)
{
    // Shader-progammable stages
    auto vertexShaderModule = gpuDevice_.createShaderModule(core::getShaderDir() / "loading_screen.vert.spv");
    auto fragmentShaderModule = gpuDevice_.createShaderModule(core::getShaderDir() / "loading_screen.frag.spv");

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
    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};

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
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
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

    auto descriptorSetLayouts = std::array{*imageDescriptor_.layout()};

    auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

    pipelineLayout_ = vk::raii::PipelineLayout(gpuDevice_.device(), pipelineLayoutInfo);

    auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo{};
    depthStencilState.depthTestEnable = false;
    depthStencilState.depthWriteEnable = false;
    depthStencilState.depthCompareOp = vk::CompareOp::eNever;
    depthStencilState.depthBoundsTestEnable = false;
    depthStencilState.stencilTestEnable = false;

    // Render passes (dynamic rendering)
    auto pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &surfaceFormat;

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
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.renderPass = nullptr;

    pipeline_ = vk::raii::Pipeline(gpuDevice_.device(), nullptr, pipelineInfo);
}
} // namespace renderer
