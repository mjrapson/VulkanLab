/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

namespace assets
{
struct MeshInstance
{
    uint32_t meshUid;
    glm::mat4 transform;
};
} // namespace assets
