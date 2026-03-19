/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/handles.h"

#include <glm/glm.hpp>

namespace renderer
{
struct DrawCommand
{
    MeshHandle meshHandle;
    MaterialHandle materialHandle;
    glm::mat4 transform;
};
} // namespace renderer
