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
GpuResourceCache::GpuResourceCache(const assets::AssetDatabase& db,
                                   const GpuDevice& gpuDevice,
                                   int maxFramesInFlight,
                                   const vk::DescriptorSetLayout& materialDescriptorSetLayout,
                                   const vk::ImageView& emptyImageView,
                                   const vk::Sampler& emptyImageSampler)
    : gpuDevice_{gpuDevice},
      maxFramesInFlight_{maxFramesInFlight},
      materialDescriptorSetLayout_{materialDescriptorSetLayout},
      emptyImageView_{emptyImageView},
      emptyImageSampler_{emptyImageSampler}
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

GpuMesh& GpuResourceCache::gpuMesh(assets::SubMesh* mesh)
{
    if (auto itr = gpuMeshes_.find(mesh); itr != gpuMeshes_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Mesh handle not uploaded to GPU");
}

const std::vector<vk::raii::DescriptorSet>& GpuResourceCache::materialDescriptorSet(assets::Material* material) const
{
    return materialDescriptorSets_.at(material);
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
        samplerInfo.magFilter = vk::Filter::eNearest;
        samplerInfo.minFilter = vk::Filter::eNearest;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
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

    createMaterialDescriptorPools(static_cast<uint32_t>(materials.size()));

    auto layouts = std::vector<vk::DescriptorSetLayout>{maxFramesInFlight_, materialDescriptorSetLayout_};

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

        auto allocInfo = vk::DescriptorSetAllocateInfo{};
        allocInfo.descriptorPool = *materialDescriptorPool_;
        allocInfo.descriptorSetCount = maxFramesInFlight_;
        allocInfo.pSetLayouts = layouts.data();

        materialDescriptorSets_[material] = std::move(vk::raii::DescriptorSets{gpuDevice_.device(), allocInfo});

        for (auto frameIndex = uint32_t{0}; frameIndex < maxFramesInFlight_; ++frameIndex)
        {
            auto bufferInfo = vk::DescriptorBufferInfo{};
            bufferInfo.buffer = materialUboBuffers_.at(frameIndex);
            bufferInfo.offset = 0;
            bufferInfo.range = stride;

            auto uboWrite = vk::WriteDescriptorSet{};
            uboWrite.dstSet = materialDescriptorSets_.at(material).at(frameIndex);
            uboWrite.dstBinding = 0;
            uboWrite.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            uboWrite.descriptorCount = 1;
            uboWrite.pBufferInfo = &bufferInfo;

            auto imageInfo = vk::DescriptorImageInfo{};
            if (material->diffuseTexture)
            {
                imageInfo.imageView = gpuImage(material->diffuseTexture).view;
                imageInfo.sampler = gpuImage(material->diffuseTexture).sampler;
            }
            else
            {
                imageInfo.imageView = emptyImageView_;
                imageInfo.sampler = emptyImageSampler_;
            }
            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            auto textureWrite = vk::WriteDescriptorSet{};
            textureWrite.dstSet = materialDescriptorSets_.at(material).at(frameIndex);
            textureWrite.dstBinding = 1;
            textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            textureWrite.descriptorCount = 1;
            textureWrite.pImageInfo = &imageInfo;

            std::array writes{uboWrite, textureWrite};
            gpuDevice_.device().updateDescriptorSets(writes, {});
        }
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
            for (const auto& subMesh : mesh->subMeshes)
            {
                totalVertices += subMesh->vertices.size();
                totalIndices += subMesh->indices.size();
            }
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
            for (const auto& subMesh : mesh->subMeshes)
            {
                auto gpuMesh = GpuMesh{};
                gpuMesh.vertexCount = static_cast<uint32_t>(subMesh->vertices.size());
                gpuMesh.indexCount = static_cast<uint32_t>(subMesh->indices.size());
                gpuMesh.vertexOffset = static_cast<uint32_t>(currentVertexOffset);
                gpuMesh.indexOffset = static_cast<uint32_t>(currentIndexOffset);

                const auto vertexSize = subMesh->vertices.size() * sizeof(core::Vertex);
                const auto indexSize = subMesh->indices.size() * sizeof(uint32_t);

                std::memcpy(static_cast<std::byte*>(vertexStagingMemory) + currentVertexOffset * sizeof(core::Vertex),
                            subMesh->vertices.data(),
                            vertexSize);

                std::memcpy(static_cast<std::byte*>(indexStagingMemory) + currentIndexOffset * sizeof(uint32_t),
                            subMesh->indices.data(),
                            indexSize);

                currentVertexOffset += subMesh->vertices.size();
                currentIndexOffset += subMesh->indices.size();

                gpuMeshes_.emplace(subMesh.get(), std::move(gpuMesh));
            }
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

void GpuResourceCache::createMaterialDescriptorPools(uint32_t materialCount)
{
    auto materialUboPoolSize = vk::DescriptorPoolSize{};
    materialUboPoolSize.type = vk::DescriptorType::eUniformBufferDynamic;
    materialUboPoolSize.descriptorCount = maxFramesInFlight_;

    auto texturePoolSize = vk::DescriptorPoolSize{};
    texturePoolSize.type = vk::DescriptorType::eCombinedImageSampler;
    texturePoolSize.descriptorCount = maxFramesInFlight_;

    auto materialPoolSizes = std::array{materialUboPoolSize, texturePoolSize};

    auto materialPoolInfo = vk::DescriptorPoolCreateInfo{};
    materialPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    materialPoolInfo.maxSets = maxFramesInFlight_ * materialCount;
    materialPoolInfo.poolSizeCount = static_cast<uint32_t>(materialPoolSizes.size());
    materialPoolInfo.pPoolSizes = materialPoolSizes.data();

    materialDescriptorPool_ = vk::raii::DescriptorPool{gpuDevice_.device(), materialPoolInfo};
}
} // namespace renderer
