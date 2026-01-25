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
                     int maxFramesInFlight,
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

    GpuImage& gpuImage(assets::Image* image);
    GpuMaterial& gpuMaterial(assets::Material* material);
    GpuMesh& gpuMesh(assets::SubMesh* mesh);
    GpuImage& gpuSkyboxImage(assets::Skybox* skybox);

    const std::vector<vk::raii::DescriptorSet>& materialDescriptorSet(assets::Material* material) const;
    const std::vector<vk::raii::DescriptorSet>& skyboxDescriptorSet(assets::Skybox* skybox) const;

  private:
    void createDefaultData();
    void uploadData(const assets::AssetDatabase& db);
    void uploadImageData(const std::vector<assets::Image*>& images);
    void uploadMaterialData(const std::vector<assets::Material*>& materials);
    void uploadMeshData(const assets::AssetDatabase& db);
    void uploadSkyboxImageData(const assets::AssetDatabase& db);

    void createMaterialDescriptorPools(uint32_t materialCount);
    void createSkyboxDescriptorPools(uint32_t skyboxCount);

  private:
    const GpuDevice& gpuDevice_;
    const int maxFramesInFlight_;
    const vk::DescriptorSetLayout& materialDescriptorSetLayout_;
    const vk::DescriptorSetLayout& skyboxDescriptorSetLayout_;
    GpuImage emptyImage_;

    vk::raii::Buffer meshVertexBuffer_{nullptr};
    vk::raii::Buffer meshIndexBuffer_{nullptr};
    vk::raii::DeviceMemory meshVertexBufferMemory_{nullptr};
    vk::raii::DeviceMemory meshIndexBufferMemory_{nullptr};

    vk::raii::DescriptorPool materialDescriptorPool_{nullptr};
    vk::raii::DescriptorPool skyboxDescriptorPool_{nullptr};
    std::unordered_map<assets::Material*, std::vector<vk::raii::DescriptorSet>> materialDescriptorSets_;
    std::unordered_map<assets::Skybox*, std::vector<vk::raii::DescriptorSet>> skyboxDescriptorSets_;
    std::vector<vk::raii::Buffer> materialUboBuffers_;
    std::vector<vk::raii::DeviceMemory> materialUboBuffersMemory_;
    std::vector<void*> materialUboMappedMemory_;

    std::unordered_map<assets::Image*, GpuImage> gpuImages_;
    std::unordered_map<assets::Material*, GpuMaterial> gpuMaterials_;
    std::unordered_map<assets::SubMesh*, GpuMesh> gpuMeshes_;
    std::unordered_map<assets::Skybox*, GpuImage> gpuSkyboxImages_;
};
} // namespace renderer
