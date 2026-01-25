/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "skybox_pass.h"

#include "private/gpu_resource_cache.h"
#include "private/shader.h"
#include "renderer/gpu_device.h"

#include <core/file_system.h>

namespace renderer
{
SkyboxPass::SkyboxPass(const GpuDevice& gpuDevice,
                       const vk::Format& surfaceFormat,
                       const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout,
                       const vk::raii::DescriptorSetLayout& skyboxDescriptorSetLayout)
    : gpuDevice_{gpuDevice}
{
    createPipeline(surfaceFormat, cameraDescriptorSetLayout, skyboxDescriptorSetLayout);
}

void SkyboxPass::recordCommands(const RenderPassCommandInfo& passInfo)
{
    const auto clearColor = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};

    auto attachmentInfo = vk::RenderingAttachmentInfo{};
    attachmentInfo.imageView = passInfo.colorImageView;
    attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    attachmentInfo.clearValue = clearColor;

    auto renderingInfo = vk::RenderingInfo{};
    renderingInfo.renderArea = {.offset = {0, 0}, .extent = passInfo.extent};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;

    passInfo.commandBuffer.beginRendering(renderingInfo);
    passInfo.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    passInfo.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                              pipelineLayout_,
                                              0,
                                              *passInfo.cameraDescriptorSet,
                                              nullptr);

    if (passInfo.skybox)
    {
        passInfo.commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipelineLayout_,
            1,
            *passInfo.gpuResourceCache.skyboxDescriptorSet(passInfo.skybox).at(passInfo.frameIndex),
            nullptr);
    }

    passInfo.commandBuffer.setViewport(0,
                                       vk::Viewport(0.0f,
                                                    0.0f,
                                                    static_cast<float>(passInfo.extent.width),
                                                    static_cast<float>(passInfo.extent.height),
                                                    0.0f,
                                                    1.0f));
    passInfo.commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), passInfo.extent));

    passInfo.commandBuffer.draw(3, 1, 0, 0);

    passInfo.commandBuffer.endRendering();
}

void SkyboxPass::createPipeline(const vk::Format& surfaceFormat,
                                const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout,
                                const vk::raii::DescriptorSetLayout& skyboxDescriptorSetLayout)
{
    // Shader-progammable stages
    auto vertexShaderModule = createShaderModule(gpuDevice_.device(),
                                                 core::readBinaryFile(core::getShaderDir() / "skybox.vert.spv"));

    auto fragmentShaderModule = createShaderModule(gpuDevice_.device(),
                                                   core::readBinaryFile(core::getShaderDir() / "skybox.frag.spv"));

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

    auto descriptorSetLayouts = std::array{*cameraDescriptorSetLayout, *skyboxDescriptorSetLayout};

    auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

    pipelineLayout_ = vk::raii::PipelineLayout(gpuDevice_.device(), pipelineLayoutInfo);

    auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo{};
    depthStencilState.depthTestEnable = true;
    depthStencilState.depthWriteEnable = false;
    depthStencilState.depthCompareOp = vk::CompareOp::eLess;
    depthStencilState.depthBoundsTestEnable = false;
    depthStencilState.stencilTestEnable = false;

    // Render passes (dynamic rendering)
    auto pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &surfaceFormat;
    pipelineRenderingCreateInfo.depthAttachmentFormat = vk::Format::eD32Sfloat;

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
