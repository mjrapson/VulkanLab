/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

#include <optional>

namespace renderer
{
struct GpuMaterialBufferData
{
    glm::vec4 diffuseColor;
    uint hasDiffuseTexture;
    uint _padding[3];
};

struct GpuMaterial
{
    uint32_t uboOffset;
};
} // namespace renderer
