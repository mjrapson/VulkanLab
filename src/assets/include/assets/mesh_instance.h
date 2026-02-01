/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/handles.h"

#include <glm/glm.hpp>

namespace assets
{
struct MeshInstance
{
    MeshHandle meshHandle;
    glm::mat4 transform;
};
} // namespace assets
