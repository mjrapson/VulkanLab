/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/handles.h"

#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>

namespace renderer
{
struct Image
{
    vk::raii::Image image{nullptr};
    vk::raii::ImageView view{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
    vk::raii::Sampler sampler{nullptr};
};
using ImageContainer = core::HandleContainer<ImageHandle, Image>;

struct Material
{
    uint32_t bufferOffset;
    vk::DeviceSize size;
    std::optional<ImageHandle> diffuseImageHandle;
    // TODO std::optional<SamplerHandle> diffuseImageSampler;
};
using MaterialContainer = core::HandleContainer<MaterialHandle, Material>;

struct MaterialUboData
{
    glm::vec4 diffuseColor;
    uint hasDiffuseTexture;
    uint _padding[3];
};

struct DirectionalLightUboData
{
    glm::vec3 direction;
    uint _padding1;
    glm::mat4 lightSpaceView;
    glm::mat4 lightSpaceProjection;
};

struct Mesh
{
    uint32_t vertexBufferOffset;
    uint32_t vertexCount;
    uint32_t indexBufferOffset;
    uint32_t indexCount;
    std::optional<MaterialHandle> materialHandle;
};
using MeshContainer = core::HandleContainer<MeshHandle, Mesh>;

struct Skybox
{
    vk::raii::Image image{nullptr};
    vk::raii::ImageView view{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
    vk::raii::Sampler sampler{nullptr};
};
using SkyboxContainer = core::HandleContainer<SkyboxHandle, Skybox>;
} // namespace renderer
