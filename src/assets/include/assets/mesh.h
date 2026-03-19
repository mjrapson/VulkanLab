/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/box.h>
#include <core/vertex.h>
#include <renderer/handles.h>

#include <optional>
#include <span>
#include <vector>

namespace assets
{
class Material;

class Mesh
{
  public:
    void setVertices(std::vector<core::Vertex>&& vertices);
    std::span<const core::Vertex> vertices() const;
    size_t vertexCount() const;

    void setIndices(std::vector<uint32_t>&& indices);
    std::span<const uint32_t> indices() const;
    size_t indexCount() const;

    const core::Box& boundingBox() const;

    void setRenderHandle(std::optional<renderer::MeshHandle> handle);
    std::optional<renderer::MeshHandle> renderHandle() const;

    void setMaterial(Material* material);
    Material* material() const;

  private:
    void updateBoundingBox();

  private:
    std::vector<core::Vertex> vertices_;
    std::vector<uint32_t> indices_;
    core::Box boundingBox_;
    std::optional<renderer::MeshHandle> renderHandle_;
    Material* material_{nullptr};
};
} // namespace assets
