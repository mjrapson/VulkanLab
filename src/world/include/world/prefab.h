/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <renderer/handles.h>

#include <glm/glm.hpp>

namespace world
{
struct MeshInstance
{
    std::vector<renderer::MeshHandle> subMeshes;
    glm::mat4 transform;
};

struct Prefab
{
    std::vector<MeshInstance> meshInstances;
};
} // namespace world
