/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "geometry_pass.h"

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
    glm::mat4 normalMatrix;
};

constexpr auto cameraDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex},
};

constexpr auto materialDescriptorBindings = std::array{
    vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eFragment},
    vk::DescriptorSetLayoutBinding{1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
};

GeometryPass::GeometryPass(const GpuDevice& gpuDevice, const vk::Format& surfaceFormat, const vk::Extent2D& extent)
    : gpuDevice_{gpuDevice},
      extent_{extent},
      cameraDescriptor_{gpuDevice_, cameraDescriptorBindings},
      materialDescriptor_{gpuDevice_, materialDescriptorBindings}
{
    resize(extent);

    createPipeline(surfaceFormat);
}

void GeometryPass::initialize(uint32_t maxFramesInFlight, std::span<BufferObject> cameraBuffers)

{
    cameraDescriptor_.resize(maxFramesInFlight);

    createCameraDescriptorSets(maxFramesInFlight, cameraBuffers);
}

void GeometryPass::resize(const vk::Extent2D& extent)
{
    extent_ = extent;

    depthImage_ = gpuDevice_.createDepthImage(extent.width, extent.height);
    depthImageMemory_ = gpuDevice_.allocateImageMemory(depthImage_, vk::MemoryPropertyFlagBits::eDeviceLocal);
    depthImageView_ = gpuDevice_.createDepthImageView(depthImage_);
}

void GeometryPass::rebuild(const std::unordered_map<MaterialHandle, Material, core::Hash<MaterialHandle>>& materials)
{
    materialDescriptor_.resize(static_cast<uint32_t>(materials.size()));
    for (const auto& [handle, material] : materials)
    {
        materialDescriptorSets_.emplace(handle, std::move(materialDescriptor_.allocateSets(1)[0]));

        auto uboWrite = vk::WriteDescriptorSet{};
        uboWrite.dstSet = *materialDescriptorSets_.at(handle);
        uboWrite.dstBinding = 0;
        uboWrite.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &material.bufferInfo;

        auto textureWrite = vk::WriteDescriptorSet{};
        textureWrite.dstSet = *materialDescriptorSets_.at(handle);
        textureWrite.dstBinding = 1;
        textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        textureWrite.descriptorCount = 1;
        textureWrite.pImageInfo = &material.imageInfo;

        std::array writes{uboWrite, textureWrite};
        gpuDevice_.device().updateDescriptorSets(writes, {});
    }
}

void GeometryPass::recordCommands(
    uint32_t frameIndex,
    const vk::raii::CommandBuffer& commandBuffer,
    const vk::raii::Buffer& vertexBuffer,
    const vk::raii::Buffer& indexBuffer,
    const std::unordered_map<MeshHandle, Mesh, core::Hash<MeshHandle>>& meshGpuData,
    const std::unordered_map<MaterialHandle, Material, core::Hash<MaterialHandle>>& materialGpuData,
    vk::ImageView colorTargetImageView,
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

    auto attachmentInfo = vk::RenderingAttachmentInfo{};
    attachmentInfo.imageView = colorTargetImageView;
    attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
    attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

    auto depthAttachmentInfo = vk::RenderingAttachmentInfo{};
    depthAttachmentInfo.imageView = depthImageView_;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachmentInfo.clearValue = clearDepth;

    auto renderingInfo = vk::RenderingInfo{};
    renderingInfo.renderArea = {.offset = {0, 0}, .extent = extent_};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     pipelineLayout_,
                                     0,
                                     *cameraDescriptorSets_.at(frameIndex),
                                     nullptr);

    commandBuffer.setViewport(
        0,
        vk::Viewport(0.0f, 0.0f, static_cast<float>(extent_.width), static_cast<float>(extent_.height), 0.0f, 1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent_));

    for (const auto& drawCommand : drawCommands)
    {
        auto& gpuMesh = meshGpuData.at(drawCommand.meshHandle);
        if (!gpuMesh.materialHandle)
        {
            continue;
        }

        auto pushConstants = PushConstants{};
        pushConstants.modelTransform = drawCommand.transform;
        pushConstants.normalMatrix = glm::transpose(glm::inverse(glm::mat3(drawCommand.transform)));

        commandBuffer.pushConstants(pipelineLayout_,
                                    vk::ShaderStageFlagBits::eVertex,
                                    0,
                                    vk::ArrayProxy<const PushConstants>{pushConstants});

        const auto& gpuMaterial = materialGpuData.at(gpuMesh.materialHandle.value());
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                         pipelineLayout_,
                                         1,
                                         *materialDescriptorSets_.at(gpuMesh.materialHandle.value()),
                                         gpuMaterial.bufferOffset);

        commandBuffer.drawIndexed(gpuMesh.indexCount, 1, gpuMesh.indexBufferOffset, gpuMesh.vertexBufferOffset, 0);
    }

    commandBuffer.endRendering();
}

void GeometryPass::createCameraDescriptorSets(uint32_t count, std::span<BufferObject> cameraBuffers)
{
    cameraDescriptorSets_ = std::move(cameraDescriptor_.allocateSets(count));

    for (auto frameIndex = uint32_t{0}; frameIndex < count; ++frameIndex)
    {
        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = cameraBuffers[frameIndex].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        auto uboWrite = vk::WriteDescriptorSet{};
        uboWrite.dstSet = cameraDescriptorSets_.at(static_cast<size_t>(frameIndex));
        uboWrite.dstBinding = 0;
        uboWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        auto writes = std::array{uboWrite};
        gpuDevice_.device().updateDescriptorSets(writes, {});
    }
}

void GeometryPass::createPipeline(const vk::Format& surfaceFormat)
{
    // Shader-progammable stages
    auto vertexShaderModule = gpuDevice_.createShaderModule(core::getShaderDir() / "basic.vert.spv");
    auto fragmentShaderModule = gpuDevice_.createShaderModule(core::getShaderDir() / "basic.frag.spv");

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

    const auto descriptorSetLayouts = std::array{*cameraDescriptor_.layout(), *materialDescriptor_.layout()};

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
