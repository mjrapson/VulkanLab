/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/gpu_image.h"
#include "renderer/gpu_material.h"
#include "renderer/gpu_mesh.h"

#include <assets/handles.h>

#include <vulkan/vulkan_raii.hpp>

#include <unordered_map>

namespace assets
{
class AssetDatabase;
}

namespace renderer
{
class GpuDevice;

class GpuResourceCache
{
    template <typename Handle, typename Resource>
    using Container = std::unordered_map<Handle, Resource, core::Hash<Handle>>;

  public:
    GpuResourceCache(const assets::AssetDatabase& db, const GpuDevice& gpuDevice);

    ~GpuResourceCache() = default;

    GpuResourceCache(const GpuResourceCache&) = delete;
    GpuResourceCache& operator=(const GpuResourceCache&) = delete;

    GpuResourceCache(GpuResourceCache&& other) = delete;
    GpuResourceCache& operator=(GpuResourceCache&& other) = delete;

    const vk::raii::Buffer& meshVertexBuffer() const;
    const vk::raii::Buffer& meshIndexBuffer() const;
    const vk::raii::Buffer& materialUboBuffer() const;

    const GpuImage& gpuImage(assets::ImageHandle handle) const;
    const GpuMaterial& gpuMaterial(assets::MaterialHandle handle) const;
    const GpuMesh& gpuMesh(assets::SubMeshHandle handle) const;
    const GpuImage& gpuSkyboxImage(assets::SkyboxHandle handle) const;

    const GpuImage& emptyImage() const;

    auto materials() const
    {
        return gpuMaterials_ | std::views::all;
    };

    auto skyboxes() const
    {
        return gpuSkyboxImages_ | std::views::all;
    }

  private:
    void createDefaultData();
    void uploadData(const assets::AssetDatabase& db);
    void uploadImageData(const assets::AssetDatabase& db);
    void uploadMaterialData(const assets::AssetDatabase& db);
    void uploadMeshData(const assets::AssetDatabase& db);
    void uploadSkyboxImageData(const assets::AssetDatabase& db);

  private:
    const GpuDevice& gpuDevice_;
    GpuImage emptyImage_;

    vk::raii::Buffer meshVertexBuffer_{nullptr};
    vk::raii::Buffer meshIndexBuffer_{nullptr};
    vk::raii::DeviceMemory meshVertexBufferMemory_{nullptr};
    vk::raii::DeviceMemory meshIndexBufferMemory_{nullptr};

    vk::raii::Buffer materialUboBuffer_{nullptr};
    vk::raii::DeviceMemory materialUboBufferMemory_{nullptr};

    Container<assets::ImageHandle, GpuImage> gpuImages_;
    Container<assets::MaterialHandle, GpuMaterial> gpuMaterials_;
    Container<assets::SubMeshHandle, GpuMesh> gpuMeshes_;
    Container<assets::SkyboxHandle, GpuImage> gpuSkyboxImages_;
};
} // namespace renderer
