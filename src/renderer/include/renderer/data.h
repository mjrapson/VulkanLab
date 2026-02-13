/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/handles.h"

#include <core/vertex.h>

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace renderer
{
struct ImageData
{
    uint32_t width;
    uint32_t height;
    uint32_t components;
    std::vector<std::byte> data;
};
using ImageDataContainer = core::HandleContainer<ImageHandle, ImageData>;

struct MaterialData
{
    glm::vec3 diffuseColour;
    std::optional<ImageHandle> diffuseImage;
};
using MaterialDataContainer = core::HandleContainer<MaterialHandle, MaterialData>;

struct MeshData
{
    std::vector<core::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::optional<MaterialHandle> materialHandle;
};
using MeshDataContainer = core::HandleContainer<MeshHandle, MeshData>;

struct SkyboxData
{
    std::array<ImageData, 6> imageData;
};
using SkyboxDataContainer = core::HandleContainer<SkyboxHandle, SkyboxData>;

struct AssetData
{
    MeshDataContainer meshData;
    MaterialDataContainer materialData;
    ImageDataContainer imageData;
    SkyboxDataContainer skyboxData;
};
} // namespace renderer
