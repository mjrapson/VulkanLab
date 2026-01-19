/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "gpu_resource_cache.h"

#include "buffer.h"
#include "image.h"
#include "memory.h"
#include "renderer/gpu_device.h"

#include <assets/asset_database.h>

#include <stdexcept>

namespace renderer
{
GpuResourceCache::GpuResourceCache(const assets::AssetDatabase& db, const GpuDevice& gpuDevice, int maxFramesInFlight)
    : gpuDevice_{gpuDevice},
      maxFramesInFlight_{maxFramesInFlight}
{
    uploadData(db);
}

const vk::raii::Buffer& GpuResourceCache::meshVertexBuffer() const
{
    return meshVertexBuffer_;
}

const vk::raii::Buffer& GpuResourceCache::meshIndexBuffer() const
{
    return meshIndexBuffer_;
}

const vk::raii::Buffer& GpuResourceCache::materialUniformBuffer(int frameIndex) const
{
    if (frameIndex < 0 || static_cast<size_t>(frameIndex) >= materialUboBuffers_.size())
    {
        throw std::runtime_error("Frame index out of bounds");
    }

    return materialUboBuffers_.at(frameIndex);
}

GpuImage& GpuResourceCache::gpuImage(assets::Image* image)
{
    if (auto itr = gpuImages_.find(image); itr != gpuImages_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Image handle not uploaded to GPU");
}

GpuMaterial& GpuResourceCache::gpuMaterial(assets::Material* material)
{
    if (auto itr = gpuMaterials_.find(material); itr != gpuMaterials_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Material handle not uploaded to GPU");
}

GpuMesh& GpuResourceCache::gpuMesh(assets::Mesh* mesh)
{
    if (auto itr = gpuMeshes_.find(mesh); itr != gpuMeshes_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Mesh handle not uploaded to GPU");
}

void GpuResourceCache::uploadData(const assets::AssetDatabase& db)
{
    auto images = std::vector<assets::Image*>{};
    for (const auto& prefab : db.prefabs())
    {
        for (const auto& image : prefab.second->images())
        {
            images.push_back(image.second.get());
        }
    }
    uploadImageData(images);

    auto materials = std::vector<assets::Material*>{};
    for (const auto& prefab : db.prefabs())
    {
        for (const auto& material : prefab.second->materials())
        {
            materials.push_back(material.second.get());
        }
    }
    uploadMaterialData(materials);

    uploadMeshData(db);
}

void GpuResourceCache::uploadImageData(const std::vector<assets::Image*>& images)
{
    for (const auto& image : images)
    {

        auto gpuImage = GpuImage{};
        gpuImage.image = createImage(gpuDevice_.device(), image->width, image->height);
        gpuImage.memory = allocateImageMemory(gpuDevice_.device(),
                                              gpuDevice_.physicalDevice(),
                                              gpuImage.image,
                                              vk::MemoryPropertyFlagBits::eDeviceLocal);

        const auto imageSize = image->width * image->height * 4; //  RGBA8
        auto stagingBuffer = createBuffer(gpuDevice_.device(),
                                          imageSize,
                                          vk::BufferUsageFlagBits::eTransferSrc,
                                          vk::SharingMode::eExclusive);

        auto stagingMemory = allocateBufferMemory(gpuDevice_.device(),
                                                  gpuDevice_.physicalDevice(),
                                                  stagingBuffer,
                                                  vk::MemoryPropertyFlagBits::eHostVisible
                                                      | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = stagingMemory.mapMemory(0, imageSize);
        std::memcpy(data, image->data.data(), imageSize);
        stagingMemory.unmapMemory();

        copyBufferToImage(gpuDevice_.device(),
                          stagingBuffer,
                          gpuImage.image,
                          gpuDevice_.graphicsQueue(),
                          gpuDevice_.commandPool(),
                          image->width,
                          image->height,
                          vk::ImageAspectFlagBits::eColor);

        gpuImage.view = createImageView(gpuDevice_.device(),
                                        gpuImage.image,
                                        vk::Format::eR8G8B8A8Srgb,
                                        vk::ImageAspectFlagBits::eColor);

        auto samplerInfo = vk::SamplerCreateInfo{};
        samplerInfo.magFilter = vk::Filter::eLinear, samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

        gpuImage.sampler = vk::raii::Sampler{gpuDevice_.device(), samplerInfo};

        gpuImages_.emplace(image, std::move(gpuImage));
    }
}

void GpuResourceCache::uploadMaterialData(const std::vector<assets::Material*>& materials)
{
    if (materials.empty())
    {
        return;
    }

    auto stride = alignMemory(sizeof(GpuMaterialBufferData),
                              gpuDevice_.physicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);

    for (auto frameIndex = 0; frameIndex < maxFramesInFlight_; ++frameIndex)
    {
        auto buffer = createBuffer(gpuDevice_.device(),
                                   stride * materials.size(),
                                   vk::BufferUsageFlagBits::eUniformBuffer,
                                   vk::SharingMode::eExclusive);

        auto memory = allocateBufferMemory(gpuDevice_.device(),
                                           gpuDevice_.physicalDevice(),
                                           buffer,
                                           vk::MemoryPropertyFlagBits::eHostVisible
                                               | vk::MemoryPropertyFlagBits::eHostCoherent);

        auto mappedMemory = memory.mapMemory(0, VK_WHOLE_SIZE);

        materialUboBuffers_.emplace_back(std::move(buffer));
        materialUboBuffersMemory_.emplace_back(std::move(memory));
        materialUboMappedMemory_.emplace_back(std::move(mappedMemory));
    }

    auto currentOffset = uint32_t{0};
    for (const auto& material : materials)
    {
        auto gpuMaterial = GpuMaterial{};
        gpuMaterial.uboOffset = currentOffset;
        gpuMaterials_.emplace(material, std::move(gpuMaterial));

        auto uboData = GpuMaterialBufferData{};
        uboData.diffuseColor = glm::vec4{material->diffuse, 1.0f};
        uboData.hasDiffuseTexture = material->diffuseTexture ? 1 : 0;

        for (auto frameIndex = 0; frameIndex < maxFramesInFlight_; ++frameIndex)
        {

            auto data = materialUboMappedMemory_.at(frameIndex);
            std::memcpy(static_cast<std::byte*>(data) + currentOffset, &uboData, sizeof(GpuMaterialBufferData));
        }

        currentOffset += static_cast<uint32_t>(stride);
    }
}

void GpuResourceCache::uploadMeshData(const assets::AssetDatabase& db)
{
    auto totalVertices = size_t{0};
    auto totalIndices = size_t{0};

    for (const auto& [_, prefab] : db.prefabs())
    {
        for (const auto& mesh : prefab->meshes())
        {
            totalVertices += mesh->vertices.size();
            totalIndices += mesh->indices.size();
        }
    }

    const auto vertexBufferSize = sizeof(core::Vertex) * totalVertices;
    meshVertexBuffer_ = createBuffer(gpuDevice_.device(),
                                     vertexBufferSize,
                                     vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                     vk::SharingMode::eExclusive);

    meshVertexBufferMemory_ = allocateBufferMemory(gpuDevice_.device(),
                                                   gpuDevice_.physicalDevice(),
                                                   meshVertexBuffer_,
                                                   vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto vertexStagingBuffer = createBuffer(gpuDevice_.device(),
                                            vertexBufferSize,
                                            vk::BufferUsageFlagBits::eTransferSrc,
                                            vk::SharingMode::eExclusive);

    auto vertexStagingBufferMemory = allocateBufferMemory(gpuDevice_.device(),
                                                          gpuDevice_.physicalDevice(),
                                                          vertexStagingBuffer,
                                                          vk::MemoryPropertyFlagBits::eHostVisible
                                                              | vk::MemoryPropertyFlagBits::eHostCoherent);

    const auto indexBufferSize = sizeof(uint32_t) * totalIndices;
    meshIndexBuffer_ = createBuffer(gpuDevice_.device(),
                                    indexBufferSize,
                                    vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                    vk::SharingMode::eExclusive);

    meshIndexBufferMemory_ = allocateBufferMemory(gpuDevice_.device(),
                                                  gpuDevice_.physicalDevice(),
                                                  meshIndexBuffer_,
                                                  vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto indexStagingBuffer = createBuffer(gpuDevice_.device(),
                                           indexBufferSize,
                                           vk::BufferUsageFlagBits::eTransferSrc,
                                           vk::SharingMode::eExclusive);

    auto indexStagingBufferMemory = allocateBufferMemory(gpuDevice_.device(),
                                                         gpuDevice_.physicalDevice(),
                                                         indexStagingBuffer,
                                                         vk::MemoryPropertyFlagBits::eHostVisible
                                                             | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* vertexStagingMemory = vertexStagingBufferMemory.mapMemory(0, vertexBufferSize);
    void* indexStagingMemory = indexStagingBufferMemory.mapMemory(0, indexBufferSize);

    auto currentVertexOffset = size_t{0};
    auto currentIndexOffset = size_t{0};
    for (const auto& [_, prefab] : db.prefabs())
    {
        for (const auto& mesh : prefab->meshes())
        {
            auto gpuMesh = GpuMesh{};
            gpuMesh.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
            gpuMesh.indexCount = static_cast<uint32_t>(mesh->indices.size());
            gpuMesh.vertexOffset = static_cast<uint32_t>(currentVertexOffset);
            gpuMesh.indexOffset = static_cast<uint32_t>(currentIndexOffset);

            const auto vertexSize = mesh->vertices.size() * sizeof(core::Vertex);
            const auto indexSize = mesh->indices.size() * sizeof(uint32_t);

            std::memcpy(static_cast<std::byte*>(vertexStagingMemory) + currentVertexOffset * sizeof(core::Vertex),
                        mesh->vertices.data(),
                        vertexSize);

            std::memcpy(static_cast<std::byte*>(indexStagingMemory) + currentIndexOffset * sizeof(uint32_t),
                        mesh->indices.data(),
                        indexSize);

            currentVertexOffset += mesh->vertices.size();
            currentIndexOffset += mesh->indices.size();

            gpuMeshes_.emplace(mesh.get(), std::move(gpuMesh));
        }
    }

    vertexStagingBufferMemory.unmapMemory();
    indexStagingBufferMemory.unmapMemory();

    copyBuffer(gpuDevice_.device(),
               vertexStagingBuffer,
               meshVertexBuffer_,
               gpuDevice_.graphicsQueue(),
               gpuDevice_.commandPool(),
               vertexBufferSize);

    copyBuffer(gpuDevice_.device(),
               indexStagingBuffer,
               meshIndexBuffer_,
               gpuDevice_.graphicsQueue(),
               gpuDevice_.commandPool(),
               indexBufferSize);
}
} // namespace renderer
