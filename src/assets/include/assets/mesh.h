/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/handles.h"

#include <core/vertex.h>

#include <memory>
#include <optional>
#include <ranges>
#include <vector>

namespace assets
{
struct SubMesh
{
    SubMesh()
        : uid{nextUid()}
    {
    }

    uint32_t uid;
    std::vector<core::Vertex> vertices;
    std::vector<uint32_t> indices;
    MaterialHandle materialHandle;

  private:
    static uint32_t nextUid()
    {
        static uint32_t nextUid = 0;
        return nextUid++;
    }
};

class Mesh
{
  public:
    Mesh();

    MeshHandle handle() const;

    void append(std::unique_ptr<SubMesh> subMesh);

    size_t subMeshCount() const;
    size_t vertexCount() const;
    size_t indexCount() const;

    auto subMeshes() const
    {
        return subMeshes_
               | std::views::transform(
                   [](const auto& ptr)
                   {
                       return *ptr;
                   });
    }

  private:
    static uint32_t nextUid();

  private:
    MeshHandle handle_;
    std::vector<std::unique_ptr<SubMesh>> subMeshes_;
};
} // namespace assets
