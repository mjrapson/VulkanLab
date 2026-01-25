/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "gpu_resource_cache.h"

#include "renderer/gpu_device.h"

#include <assets/asset_database.h>

#include <stdexcept>

namespace renderer
{
vk::DeviceSize alignMemory(vk::DeviceSize data, vk::DeviceSize alignment)
{
    if (data < alignment || data == alignment)
    {
        return alignment;
    }

    return data + (alignment - (data % alignment));
}

GpuResourceCache::GpuResourceCache(const assets::AssetDatabase& db,
                                   const GpuDevice& gpuDevice,
                                   int maxFramesInFlight,
                                   const vk::DescriptorSetLayout& materialDescriptorSetLayout,
                                   const vk::DescriptorSetLayout& skyboxDescriptorSetLayout)
    : gpuDevice_{gpuDevice},
      maxFramesInFlight_{maxFramesInFlight},
      materialDescriptorSetLayout_{materialDescriptorSetLayout},
      skyboxDescriptorSetLayout_{skyboxDescriptorSetLayout}
{
    createDefaultData();

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

GpuImage& GpuResourceCache::gpuSkyboxImage(assets::Skybox* skybox)
{
    if (auto itr = gpuSkyboxImages_.find(skybox); itr != gpuSkyboxImages_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Skybox handle not uploaded to GPU");
}

const std::vector<vk::raii::DescriptorSet>& GpuResourceCache::materialDescriptorSet(assets::Material* material) const
{
    return materialDescriptorSets_.at(material);
}

const std::vector<vk::raii::DescriptorSet>& GpuResourceCache::skyboxDescriptorSet(assets::Skybox* skybox) const
{
    return skyboxDescriptorSets_.at(skybox);
}

void GpuResourceCache::createDefaultData()
{
    emptyImage_.image = gpuDevice_.createImage(1, 1);
    emptyImage_.memory = gpuDevice_.allocateImageMemory(emptyImage_.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

    const auto imageSize = 4; //  RGBA8
    auto stagingBuffer = gpuDevice_.createBuffer(imageSize,
                                                 vk::BufferUsageFlagBits::eTransferSrc,
                                                 vk::SharingMode::eExclusive);

    auto stagingMemory = gpuDevice_.allocateBufferMemory(stagingBuffer,
                                                         vk::MemoryPropertyFlagBits::eHostVisible
                                                             | vk::MemoryPropertyFlagBits::eHostCoherent);

    auto imageData = std::vector<std::byte>{std::byte{1}, std::byte{1}, std::byte{1}, std::byte{1}};
    void* data = stagingMemory.mapMemory(0, imageSize);
    std::memcpy(data, imageData.data(), imageSize);
    stagingMemory.unmapMemory();

    auto commandBuffers = gpuDevice_.createCommandBuffers(1);
    auto& cmd = commandBuffers[0];
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    gpuDevice_.transitionImageLayout(*emptyImage_.image,
                                     *cmd,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     {}, // srcAccess
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::PipelineStageFlagBits2::eTopOfPipe,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::ImageAspectFlagBits::eColor);

    gpuDevice_.copyBufferToImage(*cmd, *stagingBuffer, *emptyImage_.image, 1, 1);

    gpuDevice_.transitionImageLayout(*emptyImage_.image,
                                     *cmd,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal,
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::AccessFlagBits2::eShaderRead,
                                     vk::PipelineStageFlagBits2::eTransfer,
                                     vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::ImageAspectFlagBits::eColor);

    cmd.end();
    gpuDevice_.submitCommandBuffer(*cmd);

    emptyImage_.view = gpuDevice_.createImageView(emptyImage_.image);
    emptyImage_.sampler = gpuDevice_.createSampler();
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

    uploadSkyboxImageData(db);
}

void GpuResourceCache::uploadImageData(const std::vector<assets::Image*>& images)
{
    for (const auto& image : images)
    {
        const auto imageSize = image->width * image->height * 4; //  RGBA8

        auto gpuImage = GpuImage{};
        gpuImage.image = gpuDevice_.createImage(image->width, image->height);
        gpuImage.memory = gpuDevice_.allocateImageMemory(gpuImage.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto stagingBuffer = gpuDevice_.createBuffer(imageSize,
                                                     vk::BufferUsageFlagBits::eTransferSrc,
                                                     vk::SharingMode::eExclusive);

        auto stagingMemory = gpuDevice_.allocateBufferMemory(stagingBuffer,
                                                             vk::MemoryPropertyFlagBits::eHostVisible
                                                                 | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* data = stagingMemory.mapMemory(0, imageSize);
        std::memcpy(data, image->data.data(), imageSize);
        stagingMemory.unmapMemory();

        auto commandBuffers = gpuDevice_.createCommandBuffers(1);
        auto& cmd = commandBuffers[0];
        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        gpuDevice_.transitionImageLayout(*gpuImage.image,
                                         *cmd,
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         {}, // srcAccess
                                         vk::AccessFlagBits2::eTransferWrite,
                                         vk::PipelineStageFlagBits2::eTopOfPipe,
                                         vk::PipelineStageFlagBits2::eTransfer,
                                         vk::ImageAspectFlagBits::eColor);

        gpuDevice_.copyBufferToImage(*cmd, *stagingBuffer, *gpuImage.image, image->width, image->height);

        gpuDevice_.transitionImageLayout(*gpuImage.image,
                                         *cmd,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal,
                                         vk::AccessFlagBits2::eTransferWrite,
                                         vk::AccessFlagBits2::eShaderRead,
                                         vk::PipelineStageFlagBits2::eTransfer,
                                         vk::PipelineStageFlagBits2::eFragmentShader,
                                         vk::ImageAspectFlagBits::eColor);

        cmd.end();
        gpuDevice_.submitCommandBuffer(*cmd);

        gpuImage.view = gpuDevice_.createImageView(gpuImage.image);
        gpuImage.sampler = gpuDevice_.createSampler();

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

    auto layouts = std::vector<vk::DescriptorSetLayout>{static_cast<size_t>(maxFramesInFlight_),
                                                        materialDescriptorSetLayout_};

    auto stride = alignMemory(sizeof(GpuMaterialBufferData),
                              gpuDevice_.physicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);

    for (auto frameIndex = 0; frameIndex < maxFramesInFlight_; ++frameIndex)
    {
        auto buffer = gpuDevice_.createBuffer(stride * materials.size(),
                                              vk::BufferUsageFlagBits::eUniformBuffer,
                                              vk::SharingMode::eExclusive);

        auto memory = gpuDevice_.allocateBufferMemory(buffer,
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

        for (auto frameIndex = uint32_t{0}; frameIndex < static_cast<uint32_t>(maxFramesInFlight_); ++frameIndex)
        {
            auto bufferInfo = vk::DescriptorBufferInfo{};
            bufferInfo.buffer = *materialUboBuffers_.at(frameIndex);
            bufferInfo.offset = 0;
            bufferInfo.range = stride;

            auto uboWrite = vk::WriteDescriptorSet{};
            uboWrite.dstSet = *materialDescriptorSets_.at(material).at(frameIndex);
            uboWrite.dstBinding = 0;
            uboWrite.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            uboWrite.descriptorCount = 1;
            uboWrite.pBufferInfo = &bufferInfo;

            auto imageInfo = vk::DescriptorImageInfo{};
            if (material->diffuseTexture)
            {
                imageInfo.imageView = *gpuImage(material->diffuseTexture).view;
                imageInfo.sampler = *gpuImage(material->diffuseTexture).sampler;
            }
            else
            {
                imageInfo.imageView = *emptyImage_.view;
                imageInfo.sampler = *emptyImage_.sampler;
            }
            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            auto textureWrite = vk::WriteDescriptorSet{};
            textureWrite.dstSet = *materialDescriptorSets_.at(material).at(frameIndex);
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
    meshVertexBuffer_ = gpuDevice_.createBuffer(vertexBufferSize,
                                                vk::BufferUsageFlagBits::eVertexBuffer
                                                    | vk::BufferUsageFlagBits::eTransferDst,
                                                vk::SharingMode::eExclusive);

    meshVertexBufferMemory_ = gpuDevice_.allocateBufferMemory(meshVertexBuffer_,
                                                              vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto vertexStagingBuffer = gpuDevice_.createBuffer(vertexBufferSize,
                                                       vk::BufferUsageFlagBits::eTransferSrc,
                                                       vk::SharingMode::eExclusive);

    auto vertexStagingBufferMemory = gpuDevice_.allocateBufferMemory(vertexStagingBuffer,
                                                                     vk::MemoryPropertyFlagBits::eHostVisible
                                                                         | vk::MemoryPropertyFlagBits::eHostCoherent);

    const auto indexBufferSize = sizeof(uint32_t) * totalIndices;
    meshIndexBuffer_ = gpuDevice_.createBuffer(indexBufferSize,
                                               vk::BufferUsageFlagBits::eIndexBuffer
                                                   | vk::BufferUsageFlagBits::eTransferDst,
                                               vk::SharingMode::eExclusive);

    meshIndexBufferMemory_ = gpuDevice_.allocateBufferMemory(meshIndexBuffer_,
                                                             vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto indexStagingBuffer = gpuDevice_.createBuffer(indexBufferSize,
                                                      vk::BufferUsageFlagBits::eTransferSrc,
                                                      vk::SharingMode::eExclusive);

    auto indexStagingBufferMemory = gpuDevice_.allocateBufferMemory(indexStagingBuffer,
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

    gpuDevice_.copyBuffer(vertexStagingBuffer, meshVertexBuffer_, vertexBufferSize);
    gpuDevice_.copyBuffer(indexStagingBuffer, meshIndexBuffer_, indexBufferSize);
}

void GpuResourceCache::uploadSkyboxImageData(const assets::AssetDatabase& db)
{
    createSkyboxDescriptorPools(static_cast<uint32_t>(db.skyboxes().size()));

    auto layouts = std::vector<vk::DescriptorSetLayout>{static_cast<size_t>(maxFramesInFlight_),
                                                        skyboxDescriptorSetLayout_};

    for (const auto& skybox : db.skyboxes())
    {
        // Assume all faces equal dimensions
        const auto width = skybox.second->images[0]->width;
        const auto height = skybox.second->images[0]->height;
        const auto imageSize = width * height * 4; // RGBA8

        auto gpuImage = GpuImage{};
        gpuImage.image = gpuDevice_.createCubemapImage(width, height);
        gpuImage.memory = gpuDevice_.allocateImageMemory(gpuImage.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto stagingBuffer = gpuDevice_.createBuffer(imageSize,
                                                     vk::BufferUsageFlagBits::eTransferSrc,
                                                     vk::SharingMode::eExclusive);

        auto stagingMemory = gpuDevice_.allocateBufferMemory(stagingBuffer,
                                                             vk::MemoryPropertyFlagBits::eHostVisible
                                                                 | vk::MemoryPropertyFlagBits::eHostCoherent);

        auto commandBuffers = gpuDevice_.createCommandBuffers(1);
        auto& cmd = commandBuffers[0];
        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        gpuDevice_.transitionImageLayout(*gpuImage.image,
                                         *cmd,
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         {}, // srcAccess
                                         vk::AccessFlagBits2::eTransferWrite,
                                         vk::PipelineStageFlagBits2::eTopOfPipe,
                                         vk::PipelineStageFlagBits2::eTransfer,
                                         vk::ImageAspectFlagBits::eColor,
                                         6);

        for (auto face = uint32_t{0}; face < 6; ++face)
        {
            void* data = stagingMemory.mapMemory(0, imageSize);
            std::memcpy(data, skybox.second->images[face]->data.data(), imageSize);
            stagingMemory.unmapMemory();

            gpuDevice_.copyBufferToImage(*cmd, *stagingBuffer, *gpuImage.image, width, height, face);
        }

        gpuDevice_.transitionImageLayout(*gpuImage.image,
                                         *cmd,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal,
                                         vk::AccessFlagBits2::eTransferWrite,
                                         vk::AccessFlagBits2::eShaderRead,
                                         vk::PipelineStageFlagBits2::eTransfer,
                                         vk::PipelineStageFlagBits2::eFragmentShader,
                                         vk::ImageAspectFlagBits::eColor,
                                         6);

        cmd.end();
        gpuDevice_.submitCommandBuffer(*cmd);

        gpuImage.view = gpuDevice_.createCubemapImageView(gpuImage.image);
        gpuImage.sampler = gpuDevice_.createSampler();

        auto allocInfo = vk::DescriptorSetAllocateInfo{};
        allocInfo.descriptorPool = *skyboxDescriptorPool_;
        allocInfo.descriptorSetCount = maxFramesInFlight_;
        allocInfo.pSetLayouts = layouts.data();

        skyboxDescriptorSets_[skybox.second.get()] = std::move(
            vk::raii::DescriptorSets{gpuDevice_.device(), allocInfo});

        for (auto frameIndex = uint32_t{0}; frameIndex < static_cast<uint32_t>(maxFramesInFlight_); ++frameIndex)
        {
            auto imageInfo = vk::DescriptorImageInfo{};
            imageInfo.imageView = gpuImage.view;
            imageInfo.sampler = gpuImage.sampler;
            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            auto textureWrite = vk::WriteDescriptorSet{};
            textureWrite.dstSet = *skyboxDescriptorSets_.at(skybox.second.get()).at(frameIndex);
            textureWrite.dstBinding = 0;
            textureWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            textureWrite.descriptorCount = 1;
            textureWrite.pImageInfo = &imageInfo;

            std::array writes{textureWrite};
            gpuDevice_.device().updateDescriptorSets(writes, {});
        }

        gpuSkyboxImages_.emplace(skybox.second.get(), std::move(gpuImage));
    }
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

void GpuResourceCache::createSkyboxDescriptorPools(uint32_t skyboxCount)
{
    auto texturePoolSize = vk::DescriptorPoolSize{};
    texturePoolSize.type = vk::DescriptorType::eCombinedImageSampler;
    texturePoolSize.descriptorCount = maxFramesInFlight_;

    auto poolSizes = std::array{texturePoolSize};

    auto poolInfo = vk::DescriptorPoolCreateInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = maxFramesInFlight_ * skyboxCount;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    skyboxDescriptorPool_ = vk::raii::DescriptorPool{gpuDevice_.device(), poolInfo};
}
} // namespace renderer
