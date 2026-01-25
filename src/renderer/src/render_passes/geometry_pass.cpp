/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "geometry_pass.h"

#include "private/gpu_resource_cache.h"
#include "private/shader.h"
#include "renderer/draw_command.h"
#include "renderer/gpu_device.h"
#include "renderer/vertex_layout.h"

#include <core/file_system.h>

namespace renderer
{
struct PushConstants
{
    glm::mat4 modelTransform;
    glm::mat4 normalMatrix;
};

GeometryPass::GeometryPass(const GpuDevice& gpuDevice,
                           const vk::Format& surfaceFormat,
                           const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout,
                           const vk::raii::DescriptorSetLayout& materialDescriptorSetLayout)
    : gpuDevice_{gpuDevice}
{
    createPipeline(surfaceFormat, cameraDescriptorSetLayout, materialDescriptorSetLayout);
}

void GeometryPass::recordCommands(const RenderPassCommandInfo& passInfo)
{
    gpuDevice_.transitionImageLayout(
        passInfo.depthImage,
        passInfo.commandBuffer,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth);

    // const auto clearColor = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
    const auto clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    auto attachmentInfo = vk::RenderingAttachmentInfo{};
    attachmentInfo.imageView = passInfo.colorImageView;
    attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
    attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    // attachmentInfo.clearValue = clearColor;

    auto depthAttachmentInfo = vk::RenderingAttachmentInfo{};
    depthAttachmentInfo.imageView = passInfo.depthImageView;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachmentInfo.clearValue = clearDepth;

    auto renderingInfo = vk::RenderingInfo{};
    renderingInfo.renderArea = {.offset = {0, 0}, .extent = passInfo.extent};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    passInfo.commandBuffer.beginRendering(renderingInfo);
    passInfo.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    passInfo.commandBuffer.bindVertexBuffers(0, *passInfo.gpuResourceCache.meshVertexBuffer(), {0});
    passInfo.commandBuffer.bindIndexBuffer(*passInfo.gpuResourceCache.meshIndexBuffer(), 0, vk::IndexType::eUint32);

    passInfo.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                              pipelineLayout_,
                                              0,
                                              *passInfo.cameraDescriptorSet,
                                              nullptr);

    passInfo.commandBuffer.setViewport(0,
                                       vk::Viewport(0.0f,
                                                    0.0f,
                                                    static_cast<float>(passInfo.extent.width),
                                                    static_cast<float>(passInfo.extent.height),
                                                    0.0f,
                                                    1.0f));
    passInfo.commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), passInfo.extent));

    for (const auto& drawCommand : passInfo.drawCommands)
    {
        auto pushConstants = PushConstants{};
        pushConstants.modelTransform = drawCommand.transform;
        pushConstants.normalMatrix = glm::transpose(glm::inverse(glm::mat3(drawCommand.transform)));

        passInfo.commandBuffer.pushConstants(pipelineLayout_,
                                             vk::ShaderStageFlagBits::eVertex,
                                             0,
                                             vk::ArrayProxy<const PushConstants>{pushConstants});

        if (drawCommand.subMesh->material)
        {
            const auto& gpuMaterial = passInfo.gpuResourceCache.gpuMaterial(drawCommand.subMesh->material);
            passInfo.commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                pipelineLayout_,
                1,
                *passInfo.gpuResourceCache.materialDescriptorSet(drawCommand.subMesh->material).at(passInfo.frameIndex),
                gpuMaterial.uboOffset);
        }

        auto& gpuMesh = passInfo.gpuResourceCache.gpuMesh(drawCommand.subMesh);
        passInfo.commandBuffer.drawIndexed(gpuMesh.indexCount, 1, gpuMesh.indexOffset, gpuMesh.vertexOffset, 0);
    }

    passInfo.commandBuffer.endRendering();
}

void GeometryPass::createPipeline(const vk::Format& surfaceFormat,
                                  const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout,
                                  const vk::raii::DescriptorSetLayout& materialDescriptorSetLayout)
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

    auto descriptorSetLayouts = std::array{*cameraDescriptorSetLayout, *materialDescriptorSetLayout};

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

    auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo{};
    depthStencilState.depthTestEnable = true;
    depthStencilState.depthWriteEnable = true;
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
