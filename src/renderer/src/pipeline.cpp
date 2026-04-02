/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/pipeline.h"

#include <core/file_system.h>
#include <core/vertex.h>

#include <numeric>

namespace renderer
{
vk::raii::ShaderModule createShaderModule(const vk::raii::Device& device, const std::filesystem::path& filePath)
{
    const auto code = core::readBinaryFile(filePath);

    auto createInfo = vk::ShaderModuleCreateInfo{};
    createInfo.codeSize = code.size() * sizeof(char);
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    return vk::raii::ShaderModule{device, createInfo};
}

Pipeline createPipeline(const vk::raii::Device& device,
                        const vk::raii::PhysicalDevice& physicalDevice,
                        const PipelineDesc& desc)
{
    // Shader program
    auto vertexShaderModule = createShaderModule(device, desc.vertexShaderPath);
    auto fragmentShaderModule = createShaderModule(device, desc.fragmentShaderPath);

    auto vertShaderStageInfo = vk::PipelineShaderStageCreateInfo{};
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = *vertexShaderModule;
    vertShaderStageInfo.pName = "vertMain";

    auto fragShaderStageInfo = vk::PipelineShaderStageCreateInfo{};
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = *fragmentShaderModule;
    fragShaderStageInfo.pName = "fragMain";

    const auto shaderStages = std::array{vertShaderStageInfo, fragShaderStageInfo};

    // Fixed function stages
    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(desc.vertexBindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = desc.vertexBindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(desc.vertexAttributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = desc.vertexAttributeDescriptions.data();

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
    rasterizer.cullMode = desc.cullMode;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = desc.depthBiasEnable;
    rasterizer.depthBiasConstantFactor = desc.depthBiasConstantFactor;
    rasterizer.depthBiasSlopeFactor = desc.depthBiasSlopeFactor;
    rasterizer.lineWidth = 1.0;

    auto multisampling = vk::PipelineMultisampleStateCreateInfo{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{};
    colorBlendAttachment.blendEnable = vk::False;
    colorBlendAttachment.colorWriteMask = desc.colourWriteMask;

    auto colorBlending = vk::PipelineColorBlendStateCreateInfo{};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const auto totalPushConstantSize = std::accumulate(desc.pushConstantRanges.begin(),
                                                       desc.pushConstantRanges.end(),
                                                       uint32_t{0},
                                                       [](auto sum, const auto& range)
                                                       {
                                                           return sum += range.size;
                                                       });

    if (totalPushConstantSize > physicalDevice.getProperties().limits.maxPushConstantsSize)
    {
        throw std::runtime_error{"Requested push constant size exceeds device limits"};
    }

    auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(desc.descriptorLayouts.size());
    pipelineLayoutInfo.pSetLayouts = desc.descriptorLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(desc.pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = desc.pushConstantRanges.data();

    auto pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo{};
    depthStencilState.depthTestEnable = desc.depthTestEnable;
    depthStencilState.depthWriteEnable = desc.depthWriteEnable;
    depthStencilState.depthCompareOp = desc.depthCompareOp;
    depthStencilState.depthBoundsTestEnable = false;
    depthStencilState.stencilTestEnable = false;

    auto pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(desc.colorAttachmentFormats.size());
    pipelineRenderingCreateInfo.pColorAttachmentFormats = desc.colorAttachmentFormats.data();
    pipelineRenderingCreateInfo.depthAttachmentFormat = desc.depthAttachmentFormat;

    auto pipelineInfo = vk::GraphicsPipelineCreateInfo{};
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = *pipelineLayout;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.renderPass = nullptr;

    auto pipeline = vk::raii::Pipeline{device, nullptr, pipelineInfo};

    return Pipeline{std::move(pipeline), std::move(pipelineLayout)};
}
} // namespace renderer
