/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>

#include <array>
#include <optional>

namespace renderer
{
constexpr auto maxTextures = 512;

struct GpuMaterialBufferData
{
    glm::vec4 diffuseColor;
};

struct GpuMaterial
{
    GpuMaterialBufferData data;
    uint32_t uboOffset;
    std::optional<uint32_t> imageIndex;

    static vk::raii::DescriptorSetLayout createDescriptorSetLayout(const vk::raii::Device& device)
    {
        auto uboLayoutBinding = vk::DescriptorSetLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        auto textureBinding = vk::DescriptorSetLayoutBinding{};
        textureBinding.binding = 1;
        textureBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        textureBinding.descriptorCount = maxTextures;
        textureBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        auto bindings = std::array<vk::DescriptorSetLayoutBinding, 2> {uboLayoutBinding, textureBinding};

        auto layoutInfo = vk::DescriptorSetLayoutCreateInfo{};
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        return vk::raii::DescriptorSetLayout(device, layoutInfo);
    }
};
} // namespace renderer
