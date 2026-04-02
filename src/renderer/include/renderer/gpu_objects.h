/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/handle.h>

#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>

namespace renderer
{
struct Image
{
    vk::raii::Image image{nullptr};
    vk::raii::ImageView view{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
};

struct Material
{
    vk::raii::Buffer uniformBuffer{nullptr};
    vk::raii::DeviceMemory uniformBufferMemory{nullptr};
    vk::DeviceSize size;
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
    vk::raii::Buffer vertexBuffer{nullptr};
    vk::raii::DeviceMemory vertexBufferMemory{nullptr};
    vk::raii::Buffer indexBuffer{nullptr};
    vk::raii::DeviceMemory indexBufferMemory{nullptr};
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
    vk::raii::Image image{nullptr};
    vk::raii::ImageView view{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
    vk::raii::DescriptorSet descriptorSet{nullptr};
};
} // namespace renderer
