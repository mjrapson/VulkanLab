/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

namespace renderer
{
struct DrawCommand
{
    uint32_t subMeshUid;
    uint32_t materialUid;
    glm::mat4 transform;
};
} // namespace renderer
