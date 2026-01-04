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

    static std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions()
    {
        auto positionAttribute = vk::VertexInputAttributeDescription{};
        positionAttribute.location = 0;
        positionAttribute.binding = 0;
        positionAttribute.format = vk::Format::eR32G32Sfloat;
        positionAttribute.offset = offsetof(core::Vertex, position);

        auto colorAttribute = vk::VertexInputAttributeDescription{};
        colorAttribute.location = 1;
        colorAttribute.binding = 0;
        colorAttribute.format = vk::Format::eR32G32B32Sfloat;
        colorAttribute.offset = offsetof(core::Vertex, color);

        return std::array<vk::VertexInputAttributeDescription, 2>{positionAttribute,
                                                                  colorAttribute};
    }
};
} // namespace renderer
