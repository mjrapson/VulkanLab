/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

namespace renderer
{
struct GpuDirectionalLight
{
    glm::vec3 direction;
    float __padding;
};
} // namespace renderer
