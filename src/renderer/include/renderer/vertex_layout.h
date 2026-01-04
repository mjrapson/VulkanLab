// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/vertex.h>

#include <glm/glm.hpp>

#include <vulkan/vulkan_raii.hpp>

#include <array>

namespace renderer
{
struct VertexLayout
{
    static vk::VertexInputBindingDescription bindingDescription()
    {
        auto bindingDescription = vk::VertexInputBindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(core::Vertex);
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions()
    {
        auto positionAttribute = vk::VertexInputAttributeDescription{};
        positionAttribute.location = 0;
        positionAttribute.binding = 0;
        positionAttribute.format = vk::Format::eR32G32B32Sfloat;
        positionAttribute.offset = offsetof(core::Vertex, position);

        auto normalAttribute = vk::VertexInputAttributeDescription{};
        normalAttribute.location = 1;
        normalAttribute.binding = 0;
        normalAttribute.format = vk::Format::eR32G32B32Sfloat;
        normalAttribute.offset = offsetof(core::Vertex, normal);

        auto textureUVAttribute = vk::VertexInputAttributeDescription{};
        textureUVAttribute.location = 2;
        textureUVAttribute.binding = 0;
        textureUVAttribute.format = vk::Format::eR32G32Sfloat;
        textureUVAttribute.offset = offsetof(core::Vertex, textureUV);

        return std::array<vk::VertexInputAttributeDescription, 3>{positionAttribute,
                                                                  normalAttribute,
                                                                  textureUVAttribute};
    }
};
} // namespace renderer
