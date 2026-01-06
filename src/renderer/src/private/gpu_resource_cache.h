/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "gpu_image.h"
#include "gpu_material.h"
#include "gpu_mesh.h"

#include <assets/asset_handle.h>
#include <assets/asset_storage.h>
#include <assets/image.h>
#include <assets/material.h>
#include <assets/mesh.h>
#include <unordered_map>

#include <vulkan/vulkan_raii.hpp>

namespace assets
{
class AssetDatabase;
}

namespace renderer
{
class GpuDevice;

class GpuResourceCache
{
  public:
    GpuResourceCache(const assets::AssetDatabase& db, const GpuDevice& gpuDevice, int maxFramesInFlight);

    ~GpuResourceCache() = default;

    GpuResourceCache(const GpuResourceCache&) = delete;
    GpuResourceCache& operator=(const GpuResourceCache&) = delete;

    GpuResourceCache(GpuResourceCache&& other) = default;
    GpuResourceCache& operator=(GpuResourceCache&& other) = default;

    const vk::raii::Buffer& meshVertexBuffer() const;
    const vk::raii::Buffer& meshIndexBuffer() const;
    const vk::raii::Buffer& materialUniformBuffer(int frameIndex) const;

    GpuImage& gpuImage(const assets::AssetHandle<assets::Image>& handle);
    GpuMaterial& gpuMaterial(const assets::AssetHandle<assets::Material>& handle);
    GpuMesh& gpuMesh(const assets::AssetHandle<assets::Mesh>& handle);

  private:
    void uploadData(const assets::AssetDatabase& db);
    void uploadImageData(const assets::AssetStorage<assets::Image>& images);
    void uploadMaterialData(const assets::AssetStorage<assets::Material>& materials);
    void uploadMeshData(const assets::AssetStorage<assets::Mesh>& meshes);

  private:
    const GpuDevice& gpuDevice_;
    const int maxFramesInFlight_;

    vk::raii::Buffer meshVertexBuffer_{nullptr};
    vk::raii::Buffer meshIndexBuffer_{nullptr};
    vk::raii::DeviceMemory meshVertexBufferMemory_{nullptr};
    vk::raii::DeviceMemory meshIndexBufferMemory_{nullptr};

    std::vector<vk::raii::Buffer> materialUboBuffers_;
    std::vector<vk::raii::DeviceMemory> materialUboBuffersMemory_;
    std::vector<void*> materialUboMappedMemory_;

    std::unordered_map<assets::AssetHandle<assets::Image>, GpuImage> gpuImages_;
    std::unordered_map<assets::AssetHandle<assets::Material>, GpuMaterial> gpuMaterials_;
    std::unordered_map<assets::AssetHandle<assets::Mesh>, GpuMesh> gpuMeshes_;
};
} // namespace renderer
