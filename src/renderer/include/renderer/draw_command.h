/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <assets/handles.h>

#include <glm/glm.hpp>

namespace renderer
{
struct DrawCommand
{
    assets::SubMeshHandle subMeshHandle;
    assets::MaterialHandle materialHandle;
    glm::mat4 transform;
};
} // namespace renderer
