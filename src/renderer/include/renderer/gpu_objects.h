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
    ImageHandle hdrImage;
    ImageHandle cubemapImage;
    bool initialized{false};
    vk::raii::DescriptorSet descriptorSet{nullptr};
    vk::raii::DescriptorSet preProcessDescriptorSet{nullptr};
};
} // namespace renderer
