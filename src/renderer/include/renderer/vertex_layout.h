// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/vertex.h>

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
constexpr auto vertexBindingDescription = vk::VertexInputBindingDescription{0,
                                                                            sizeof(core::Vertex),
                                                                            vk::VertexInputRate::eVertex};
constexpr auto vertexPositionAttribute =
    vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(core::Vertex, position)};
constexpr auto vertexNormalAttribute =
    vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat, offsetof(core::Vertex, normal)};
constexpr auto vertexTextureUVAttribute =
    vk::VertexInputAttributeDescription{2, 0, vk::Format::eR32G32Sfloat, offsetof(core::Vertex, textureUV)};

} // namespace renderer
