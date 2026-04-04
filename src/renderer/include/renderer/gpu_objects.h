/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/handles.h"

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
struct Material
{
    BufferHandle uniformBuffer;
    vk::raii::DescriptorSet descriptorSet{nullptr};
};

struct Mesh
{
    BufferHandle vertexBuffer;
    BufferHandle indexBuffer;
    uint32_t indexCount{};
};

struct Skybox
{
    ImageHandle image;
    vk::raii::DescriptorSet descriptorSet{nullptr};
};

struct LoadingScreen
{
    ImageHandle image;
    vk::raii::DescriptorSet descriptorSet{nullptr};
};
} // namespace renderer
