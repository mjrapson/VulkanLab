/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "skybox_pass.h"

#include "private/gpu_resource_cache.h"
#include "renderer/gpu_device.h"

#include <core/file_system.h>

namespace renderer
{
SkyboxPass::SkyboxPass(const GpuDevice& gpuDevice)
    : gpuDevice_{gpuDevice}
{
}

void SkyboxPass::initialize(const vk::Extent2D& extent,
                            const vk::Format& surfaceFormat,
                            uint32_t maxFramesInFlight,
                            const std::vector<vk::raii::Buffer>& cameraBuffers)
{
    resize(extent);

    createDescriptorSetLayouts();

    createPipeline(surfaceFormat);

    createCameraDescriptorPool(maxFramesInFlight);
    createCameraDescriptorSets(maxFramesInFlight, cameraBuffers);
}

void SkyboxPass::resize(const vk::Extent2D& extent)
{
    extent_ = extent;
}

void SkyboxPass::rebuild(const GpuResourceCache& resourceCache)
{
    recreateDescriptorPools(static_cast<uint32_t>(resourceCache.skyboxes().size()));
    recreateDescriptorSets(resourceCache);
}

void SkyboxPass::recordCommands(const RenderPassCommandInfo& passInfo,
                                vk::ImageView colorTargetImageView)
{
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

    passInfo.commandBuffer.beginRendering(renderingInfo);
    passInfo.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    passInfo.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                              pipelineLayout_,
                                              0,
                                              *cameraDescriptorSets_.at(passInfo.frameIndex),
                                              nullptr);

    if (passInfo.skyboxHandle)
    {
        passInfo.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                  pipelineLayout_,
                                                  1,
                                                  *skyboxDescriptorSets_.at(passInfo.skyboxHandle.value()),
                                                  nullptr);
    }

    passInfo.commandBuffer.setViewport(
        0,
        vk::Viewport(0.0f, 0.0f, static_cast<float>(extent_.width), static_cast<float>(extent_.height), 0.0f, 1.0f));
    passInfo.commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent_));

    passInfo.commandBuffer.draw(36, 1, 0, 0);

    passInfo.commandBuffer.endRendering();
}

void SkyboxPass::createDescriptorSetLayouts()
{
    auto cameraLayoutBinding = vk::DescriptorSetLayoutBinding{};
    cameraLayoutBinding.binding = 0;
    cameraLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    cameraLayoutBinding.descriptorCount = 1;
    cameraLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    auto cameraLayoutBindings = std::array{cameraLayoutBinding};
    cameraDescriptorSetLayout_ = gpuDevice_.createDescriptorSetLayout(cameraLayoutBindings);

    auto skyboxTextureBinding = vk::DescriptorSetLayoutBinding{};
    skyboxTextureBinding.binding = 0;
    skyboxTextureBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    skyboxTextureBinding.descriptorCount = 1;
    skyboxTextureBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    auto skyboxLayoutBindings = std::array{skyboxTextureBinding};
    skyboxDescriptorSetLayout_ = gpuDevice_.createDescriptorSetLayout(skyboxLayoutBindings);
}

void SkyboxPass::createCameraDescriptorPool(uint32_t count)
{
    auto cameraPoolSize = vk::DescriptorPoolSize{};
    cameraPoolSize.type = vk::DescriptorType::eUniformBuffer;
    cameraPoolSize.descriptorCount = count;

    auto cameraPoolSizes = std::array{cameraPoolSize};

    auto cameraPoolInfo = vk::DescriptorPoolCreateInfo{};
    cameraPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    cameraPoolInfo.maxSets = count;
    cameraPoolInfo.poolSizeCount = static_cast<uint32_t>(cameraPoolSizes.size());
    cameraPoolInfo.pPoolSizes = cameraPoolSizes.data();

    cameraDescriptorPool_ = vk::raii::DescriptorPool{gpuDevice_.device(), cameraPoolInfo};
}

void SkyboxPass::createCameraDescriptorSets(uint32_t count, const std::vector<vk::raii::Buffer>& cameraBuffers)
{
    auto layouts = std::vector<vk::DescriptorSetLayout>{count, *cameraDescriptorSetLayout_};

    auto allocInfo = vk::DescriptorSetAllocateInfo{};
    allocInfo.descriptorPool = *cameraDescriptorPool_;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts.data();

    cameraDescriptorSets_ = std::move(vk::raii::DescriptorSets{gpuDevice_.device(), allocInfo});

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

void SkyboxPass::createPipeline(const vk::Format& surfaceFormat)
{
    // Shader-progammable stages
    auto vertexShaderModule = gpuDevice_.createShaderModule(core::getShaderDir() / "skybox.vert.spv");
    auto fragmentShaderModule = gpuDevice_.createShaderModule(core::getShaderDir() / "skybox.frag.spv");

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

    auto descriptorSetLayouts = std::array{*cameraDescriptorSetLayout_, *skyboxDescriptorSetLayout_};

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

void SkyboxPass::recreateDescriptorPools(uint32_t count)
{
    auto texturePoolSize = vk::DescriptorPoolSize{};
    texturePoolSize.type = vk::DescriptorType::eCombinedImageSampler;
    texturePoolSize.descriptorCount = 1;

    auto poolInfo = vk::DescriptorPoolCreateInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = count;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &texturePoolSize;

    skyboxDescriptorPool_ = vk::raii::DescriptorPool{gpuDevice_.device(), poolInfo};
}

void SkyboxPass::recreateDescriptorSets(const GpuResourceCache& resourceCache)
{
    for (const auto& [handle, image] : resourceCache.skyboxes())
    {
        auto allocInfo = vk::DescriptorSetAllocateInfo{};
        allocInfo.descriptorPool = *skyboxDescriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &*skyboxDescriptorSetLayout_;

        auto sets = vk::raii::DescriptorSets{gpuDevice_.device(), allocInfo};
        skyboxDescriptorSets_.emplace(handle, std::move(sets[0]));

        auto imageInfo = vk::DescriptorImageInfo{};
        imageInfo.imageView = image.view;
        imageInfo.sampler = image.sampler;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        auto textureWrite = vk::WriteDescriptorSet{};
        textureWrite.dstSet = *skyboxDescriptorSets_.at(handle);
        textureWrite.dstBinding = 0;
        textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        textureWrite.descriptorCount = 1;
        textureWrite.pImageInfo = &imageInfo;

        std::array writes{textureWrite};
        gpuDevice_.device().updateDescriptorSets(writes, {});
    }
}
} // namespace renderer
