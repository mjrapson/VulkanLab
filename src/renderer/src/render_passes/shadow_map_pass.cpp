/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "shadow_map_pass.h"

#include "renderer/draw_command.h"
#include "renderer/gpu_device.h"
#include "renderer/vertex_layout.h"

#include <core/file_system.h>

#include <array>

namespace renderer
{
struct PushConstants
{
    glm::mat4 modelTransform;
};

constexpr auto directionalLightDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex},
};

constexpr auto shadowMapSize = uint32_t{2048};

ShadowMapPass::ShadowMapPass(const GpuDevice& gpuDevice)
    : gpuDevice_{gpuDevice},
      directionalLightDescriptor_{gpuDevice_, directionalLightDescriptorBindings}
{
    createPipeline();

    depthImage_ = gpuDevice_.createDepthImage(shadowMapSize, shadowMapSize);
    depthImageMemory_ = gpuDevice_.allocateImageMemory(depthImage_, vk::MemoryPropertyFlagBits::eDeviceLocal);
    depthImageView_ = gpuDevice_.createDepthImageView(depthImage_);
}

void ShadowMapPass::regenerateDescriptorSets(const DirectionalLight& directionalLight)
{
    // Directional light descriptor sets
    directionalLightDescriptor_.resize(1);
    directionalLightDescriptorSet_ = std::move(directionalLightDescriptor_.allocateSets(1)[0]);

    auto dirLightUboWrite = vk::WriteDescriptorSet{};
    dirLightUboWrite.dstSet = *directionalLightDescriptorSet_;
    dirLightUboWrite.dstBinding = 0;
    dirLightUboWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    dirLightUboWrite.descriptorCount = 1;
    dirLightUboWrite.pBufferInfo = &directionalLight.bufferInfo;

    auto dirLightWrite = std::array{dirLightUboWrite};
    gpuDevice_.device().updateDescriptorSets(dirLightWrite, {});
}

void ShadowMapPass::recordCommands(const vk::raii::CommandBuffer& commandBuffer,
                                   const vk::raii::Buffer& vertexBuffer,
                                   const vk::raii::Buffer& indexBuffer,
                                   const std::unordered_map<MeshHandle, Mesh, core::Hash<MeshHandle>>& meshGpuData,
                                   std::span<const DrawCommand> drawCommands)
{
    gpuDevice_.transitionImageLayout(
        depthImage_,
        commandBuffer,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth);

    const auto clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    auto depthAttachmentInfo = vk::RenderingAttachmentInfo{};
    depthAttachmentInfo.imageView = depthImageView_;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachmentInfo.clearValue = clearDepth;

    auto renderingInfo = vk::RenderingInfo{};
    renderingInfo.renderArea = {.offset = {0, 0}, .extent = vk::Extent2D{shadowMapSize, shadowMapSize}};
    renderingInfo.layerCount = 1;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

    commandBuffer.setViewport(
        0,
        vk::Viewport(0.0f, 0.0f, static_cast<float>(shadowMapSize), static_cast<float>(shadowMapSize), 0.0f, 1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D{shadowMapSize, shadowMapSize}));

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     *pipelineLayout_,
                                     0,
                                     *directionalLightDescriptorSet_,
                                     nullptr);

    for (const auto& drawCommand : drawCommands)
    {
        auto& gpuMesh = meshGpuData.at(drawCommand.meshHandle);

        auto pushConstants = PushConstants{};
        pushConstants.modelTransform = drawCommand.transform;

        commandBuffer.pushConstants(pipelineLayout_,
                                    vk::ShaderStageFlagBits::eVertex,
                                    0,
                                    vk::ArrayProxy<const PushConstants>{pushConstants});

        commandBuffer.drawIndexed(gpuMesh.indexCount, 1, gpuMesh.indexBufferOffset, gpuMesh.vertexBufferOffset, 0);
    }

    commandBuffer.endRendering();
}

void ShadowMapPass::createPipeline()
{
    // Shader-progammable stages
    auto vertexShaderModule = gpuDevice_.createShaderModule(core::getShaderDir() / "shadowmap.vert.spv");
    auto fragmentShaderModule = gpuDevice_.createShaderModule(core::getShaderDir() / "shadowmap.frag.spv");

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
    auto positionAttribute = vk::VertexInputAttributeDescription{};
    positionAttribute.location = 0;
    positionAttribute.binding = 0;
    positionAttribute.format = vk::Format::eR32G32B32Sfloat;
    positionAttribute.offset = offsetof(core::Vertex, position);

    const auto bindingDescriptions = VertexLayout::bindingDescription();
    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptions;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &positionAttribute;

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
    rasterizer.depthBiasEnable = vk::True;
    rasterizer.depthBiasConstantFactor = 0.0f; // 1.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;    // 1.5f;
    rasterizer.lineWidth = 1.0;

    auto multisampling = vk::PipelineMultisampleStateCreateInfo{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{};
    colorBlendAttachment.blendEnable = vk::False;
    colorBlendAttachment.colorWriteMask = {};

    auto colorBlending = vk::PipelineColorBlendStateCreateInfo{};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const auto descriptorSetLayouts = std::array{*directionalLightDescriptor_.layout()};

    if (gpuDevice_.exceedsPushConstantLimit(sizeof(PushConstants)))
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

    auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo{};
    depthStencilState.depthTestEnable = true;
    depthStencilState.depthWriteEnable = true;
    depthStencilState.depthCompareOp = vk::CompareOp::eLess;
    depthStencilState.depthBoundsTestEnable = false;
    depthStencilState.stencilTestEnable = false;

    // Render passes (dynamic rendering)
    auto pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.colorAttachmentCount = 0;
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
