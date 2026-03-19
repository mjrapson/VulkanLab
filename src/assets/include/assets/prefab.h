/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/image.h"
#include "assets/material.h"
#include "assets/mesh.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace assets
{
struct MeshInstance
{
    std::vector<Mesh*> subMeshes;
    glm::mat4 transform;
};

struct Prefab
{
    std::vector<MeshInstance> meshInstances;
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Material>> materials;
    std::vector<std::unique_ptr<Image>> images;
};
} // namespace assets
