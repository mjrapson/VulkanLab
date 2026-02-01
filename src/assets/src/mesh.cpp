/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/mesh.h"

#include <stdexcept>

namespace assets
{
Mesh::Mesh()
    : handle_{nextUid()}
{
}

MeshHandle Mesh::handle() const
{
    return handle_;
}

void Mesh::append(std::unique_ptr<SubMesh> subMesh)
{
    if (!subMesh)
    {
        return;
    }

    subMeshes_.push_back(std::move(subMesh));
}

size_t Mesh::vertexCount() const
{
    return std::ranges::fold_left(subMeshes_,
                                  size_t{0},
                                  [](size_t n, const auto& subMesh)
                                  {
                                      return n + subMesh->vertices.size();
                                  });
}

size_t Mesh::indexCount() const
{
    return std::ranges::fold_left(subMeshes_,
                                  size_t{0},
                                  [](size_t n, const auto& subMesh)
                                  {
                                      return n + subMesh->indices.size();
                                  });
}

uint32_t Mesh::nextUid()
{
    static uint32_t uid = 0;
    return uid++;
}
} // namespace assets
