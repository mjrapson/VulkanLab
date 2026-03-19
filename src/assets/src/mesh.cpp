/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/mesh.h"

namespace assets
{
void Mesh::setVertices(std::vector<core::Vertex>&& vertices)
{
    vertices_ = std::move(vertices);

    updateBoundingBox();
}

std::span<const core::Vertex> Mesh::vertices() const
{
    return vertices_;
}

size_t Mesh::vertexCount() const
{
    return vertices_.size();
}

void Mesh::setIndices(std::vector<uint32_t>&& indices)
{
    indices_ = std::move(indices);
}

std::span<const uint32_t> Mesh::indices() const
{
    return indices_;
}

size_t Mesh::indexCount() const
{
    return indices_.size();
}

const core::Box& Mesh::boundingBox() const
{
    return boundingBox_;
}

void Mesh::setRenderHandle(std::optional<renderer::MeshHandle> handle)
{
    renderHandle_ = handle;
}

std::optional<renderer::MeshHandle> Mesh::renderHandle() const
{
    return renderHandle_;
}

void Mesh::setMaterial(Material* material)
{
    material_ = material;
}

Material* Mesh::material() const
{
    return material_;
}

void Mesh::updateBoundingBox()
{
    // TODO...
}
} // namespace assets
