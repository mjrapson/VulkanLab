/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/material.h"
#include "assets/mesh.h"

#include <core/handle.h>

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace assets
{
struct MeshInstance
{
    core::Handle<Mesh> meshHandle;
    std::optional<core::Handle<Material>> materialHandle;
};

struct Prefab
{
    std::vector<MeshInstance> meshInstances;
};
} // namespace assets
