/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "geometry_pass.h"

#include "renderer/draw_command.h"
#include "renderer/gpu_device.h"
#include "renderer/gpu_resource_cache.h"
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

GeometryPass::GeometryPass(const GpuDevice& gpuDevice)
    : gpuDevice_{gpuDevice},
      cameraDescriptor_{gpuDevice_, cameraDescriptorBindings},
      materialDescriptor_{gpuDevice_, materialDescriptorBindings}
{
}

void GeometryPass::initialize(const vk::Extent2D& extent,
                              const vk::Format& surfaceFormat,
                              uint32_t maxFramesInFlight,
                              const std::vector<vk::raii::Buffer>& cameraBuffers)

{
    resize(extent);

    createPipeline(surfaceFormat);

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

void GeometryPass::rebuild(const GpuResourceCache& resourceCache)
{
    materialDescriptor_.resize(static_cast<uint32_t>(resourceCache.materials().size()));

    recreateMaterialDescriptorSets(resourceCache);
}

void GeometryPass::recordCommands(const RenderPassCommandInfo& passInfo, vk::ImageView colorTargetImageView)
{
    gpuDevice_.transitionImageLayout(
        depthImage_,
        passInfo.commandBuffer,
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

    passInfo.commandBuffer.beginRendering(renderingInfo);
    passInfo.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    passInfo.commandBuffer.bindVertexBuffers(0, *passInfo.gpuResourceCache.meshVertexBuffer(), {0});
    passInfo.commandBuffer.bindIndexBuffer(*passInfo.gpuResourceCache.meshIndexBuffer(), 0, vk::IndexType::eUint32);

    passInfo.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                              pipelineLayout_,
                                              0,
                                              *cameraDescriptorSets_.at(passInfo.frameIndex),
                                              nullptr);

    passInfo.commandBuffer.setViewport(
        0,
        vk::Viewport(0.0f, 0.0f, static_cast<float>(extent_.width), static_cast<float>(extent_.height), 0.0f, 1.0f));
    passInfo.commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent_));

    for (const auto& drawCommand : passInfo.drawCommands)
    {
        auto pushConstants = PushConstants{};
        pushConstants.modelTransform = drawCommand.transform;
        pushConstants.normalMatrix = glm::transpose(glm::inverse(glm::mat3(drawCommand.transform)));

        passInfo.commandBuffer.pushConstants(pipelineLayout_,
                                             vk::ShaderStageFlagBits::eVertex,
                                             0,
                                             vk::ArrayProxy<const PushConstants>{pushConstants});

        const auto& gpuMaterial = passInfo.gpuResourceCache.gpuMaterial(drawCommand.materialHandle);
        passInfo.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                  pipelineLayout_,
                                                  1,
                                                  *materialDescriptorSets_.at(drawCommand.materialHandle),
                                                  gpuMaterial.uboOffset);

        auto& gpuMesh = passInfo.gpuResourceCache.gpuMesh(drawCommand.subMeshHandle);
        passInfo.commandBuffer.drawIndexed(gpuMesh.indexCount, 1, gpuMesh.indexOffset, gpuMesh.vertexOffset, 0);
    }

    passInfo.commandBuffer.endRendering();
}

void GeometryPass::createCameraDescriptorSets(uint32_t count, const std::vector<vk::raii::Buffer>& cameraBuffers)
{
    cameraDescriptorSets_ = std::move(cameraDescriptor_.allocateSets(count));

    for (auto frameIndex = uint32_t{0}; frameIndex < count; ++frameIndex)
    {
        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = cameraBuffers.at(frameIndex);
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

void GeometryPass::recreateMaterialDescriptorSets(const GpuResourceCache& resourceCache)
{
    const auto stride = gpuDevice_.calculateAlignedUboStride(sizeof(GpuMaterialBufferData));
    for (const auto& [handle, material] : resourceCache.materials())
    {
        materialDescriptorSets_.emplace(handle, std::move(materialDescriptor_.allocateSets(1)[0]));

        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = *resourceCache.materialUboBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = stride;

        auto uboWrite = vk::WriteDescriptorSet{};
        uboWrite.dstSet = *materialDescriptorSets_.at(handle);
        uboWrite.dstBinding = 0;
        uboWrite.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        auto imageInfo = vk::DescriptorImageInfo{};
        if (material.diffuseImageHandle)
        {
            imageInfo.imageView = *resourceCache.gpuImage(material.diffuseImageHandle.value()).view;
            imageInfo.sampler = *resourceCache.gpuImage(material.diffuseImageHandle.value()).sampler;
        }
        else
        {
            imageInfo.imageView = *resourceCache.emptyImage().view;
            imageInfo.sampler = *resourceCache.emptyImage().sampler;
        }
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        auto textureWrite = vk::WriteDescriptorSet{};
        textureWrite.dstSet = *materialDescriptorSets_.at(handle);
        textureWrite.dstBinding = 1;
        textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        textureWrite.descriptorCount = 1;
        textureWrite.pImageInfo = &imageInfo;

        std::array writes{uboWrite, textureWrite};
        gpuDevice_.device().updateDescriptorSets(writes, {});
    }
}
} // namespace renderer
