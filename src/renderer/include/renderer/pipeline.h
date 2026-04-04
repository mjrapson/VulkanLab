/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <filesystem>
#include <vector>

namespace renderer
{
struct Pipeline
{
    vk::raii::Pipeline pipeline{nullptr};
    vk::raii::PipelineLayout layout{nullptr};
};

struct PipelineDesc
{
    std::filesystem::path vertexShaderPath{};
    std::filesystem::path fragmentShaderPath{};
    std::vector<vk::VertexInputBindingDescription> vertexBindingDescriptions{};
    std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions{};
    std::vector<vk::DescriptorSetLayout> descriptorLayouts{};
    std::vector<vk::PushConstantRange> pushConstantRanges{};
    std::vector<vk::Format> colorAttachmentFormats{};
    vk::Format depthAttachmentFormat{};
    vk::Bool32 depthBiasEnable{vk::False};
    float depthBiasConstantFactor{};
    float depthBiasSlopeFactor{};
    vk::Bool32 depthTestEnable{vk::True};
    vk::Bool32 depthWriteEnable{vk::True};
    vk::CompareOp depthCompareOp{vk::CompareOp::eLess};
    vk::CullModeFlags cullMode{vk::CullModeFlagBits::eBack};
    vk::ColorComponentFlags colourWriteMask{vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
};

Pipeline createPipeline(const vk::raii::Device& device,
                        const vk::raii::PhysicalDevice& physicalDevice,
                        const PipelineDesc& desc);

Pipeline createComputePipeline(const vk::raii::Device& device,
                               std::filesystem::path computeShaderPath,
                               std::vector<vk::DescriptorSetLayout> descriptorLayouts);
} // namespace renderer
