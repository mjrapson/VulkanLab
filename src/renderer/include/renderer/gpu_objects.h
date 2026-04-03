/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/buffer.h"
#include "renderer/image.h"

#include <core/handle.h>

#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>

namespace renderer
{
struct Material
{
    core::Handle<Buffer> uniformBuffer;
    vk::raii::DescriptorSet descriptorSet{nullptr};
};

struct MaterialUboData
{
    glm::vec4 diffuseColor;
    uint32_t hasDiffuseTexture;
    uint32_t _padding;
};

struct DirectionalLightUboData
{
    glm::vec3 direction;
    uint32_t _padding1;
    glm::mat4 lightSpaceView;
    glm::mat4 lightSpaceProjection;
};

struct Mesh
{
    core::Handle<Buffer> vertexBuffer;
    core::Handle<Buffer> indexBuffer;
    uint32_t indexCount{};
};

struct Skybox
{
    vk::raii::Image image{nullptr};
    vk::raii::ImageView view{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
    vk::raii::DescriptorSet descriptorSet{nullptr};
};

struct LoadingScreen
{
    core::Handle<Image> image;
    vk::raii::DescriptorSet descriptorSet{nullptr};
};
} // namespace renderer
