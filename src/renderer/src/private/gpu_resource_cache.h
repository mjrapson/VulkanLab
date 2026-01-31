/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "gpu_image.h"
#include "gpu_material.h"
#include "gpu_mesh.h"

#include <assets/image.h>
#include <assets/material.h>
#include <assets/mesh.h>
#include <assets/skybox.h>
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
    GpuResourceCache(const assets::AssetDatabase& db,
                     const GpuDevice& gpuDevice,
                     const vk::DescriptorSetLayout& materialDescriptorSetLayout,
                     const vk::DescriptorSetLayout& skyboxDescriptorSetLayout);

    ~GpuResourceCache() = default;

    GpuResourceCache(const GpuResourceCache&) = delete;
    GpuResourceCache& operator=(const GpuResourceCache&) = delete;

    GpuResourceCache(GpuResourceCache&& other) = default;
    GpuResourceCache& operator=(GpuResourceCache&& other) = default;

    const vk::raii::Buffer& meshVertexBuffer() const;
    const vk::raii::Buffer& meshIndexBuffer() const;
    const vk::raii::Buffer& materialUniformBuffer(int frameIndex) const;

    GpuImage& gpuImage(uint32_t handle);
    GpuMaterial& gpuMaterial(uint32_t handle);
    GpuMesh& gpuMesh(uint32_t handle);
    GpuImage& gpuSkyboxImage(uint32_t handle);

    const vk::raii::DescriptorSet& materialDescriptorSet(uint32_t handle) const;
    const vk::raii::DescriptorSet& skyboxDescriptorSet(uint32_t handle) const;

  private:
    void createDefaultData();
    void uploadData(const assets::AssetDatabase& db);
    void uploadImageData(const assets::AssetDatabase& db);
    void uploadMaterialData(const assets::AssetDatabase& db);
    void uploadMeshData(const assets::AssetDatabase& db);
    void uploadSkyboxImageData(const assets::AssetDatabase& db);

    void createMaterialDescriptorPools(uint32_t materialCount);
    void createSkyboxDescriptorPools(uint32_t skyboxCount);

  private:
    const GpuDevice& gpuDevice_;
    const vk::DescriptorSetLayout& materialDescriptorSetLayout_;
    const vk::DescriptorSetLayout& skyboxDescriptorSetLayout_;
    GpuImage emptyImage_;

    vk::raii::Buffer meshVertexBuffer_{nullptr};
    vk::raii::Buffer meshIndexBuffer_{nullptr};
    vk::raii::DeviceMemory meshVertexBufferMemory_{nullptr};
    vk::raii::DeviceMemory meshIndexBufferMemory_{nullptr};

    vk::raii::DescriptorPool materialDescriptorPool_{nullptr};
    vk::raii::DescriptorPool skyboxDescriptorPool_{nullptr};
    std::unordered_map<uint32_t, vk::raii::DescriptorSet> materialDescriptorSets_;
    std::unordered_map<uint32_t, vk::raii::DescriptorSet> skyboxDescriptorSets_;
    vk::raii::Buffer materialUboBuffer_{nullptr};
    vk::raii::DeviceMemory materialUboBufferMemory_{nullptr};
    void* materialUboMappedMemory_{nullptr};

    std::unordered_map<uint32_t, GpuImage> gpuImages_;
    std::unordered_map<uint32_t, GpuMaterial> gpuMaterials_;
    std::unordered_map<uint32_t, GpuMesh> gpuMeshes_;
    std::unordered_map<uint32_t, GpuImage> gpuSkyboxImages_;
};
} // namespace renderer
