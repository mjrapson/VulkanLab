/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "gpu_image.h"
#include "gpu_material.h"
#include "gpu_mesh.h"

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
    const GpuMesh& gpuMesh(uint32_t handle) const;
    const GpuImage& gpuSkyboxImage(assets::SkyboxHandle handle) const;

    const GpuImage& emptyImage() const;

    const std::unordered_map<assets::MaterialHandle, GpuMaterial>& materials() const;
    const std::unordered_map<assets::SkyboxHandle, GpuImage>& skyboxes() const;

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

    std::unordered_map<assets::ImageHandle, GpuImage> gpuImages_;
    std::unordered_map<assets::MaterialHandle, GpuMaterial> gpuMaterials_;
    std::unordered_map<uint32_t, GpuMesh> gpuMeshes_;
    std::unordered_map<assets::SkyboxHandle, GpuImage> gpuSkyboxImages_;
};
} // namespace renderer
