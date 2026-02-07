/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "gpu_resource_cache.h"

#include "renderer/gpu_device.h"

#include <assets/asset_database.h>

#include <stdexcept>

namespace renderer
{
GpuResourceCache::GpuResourceCache(const assets::AssetDatabase& db, const GpuDevice& gpuDevice)
    : gpuDevice_{gpuDevice}
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

const vk::raii::Buffer& GpuResourceCache::materialUboBuffer() const
{
    return materialUboBuffer_;
}

const GpuImage& GpuResourceCache::gpuImage(assets::ImageHandle handle) const
{
    if (auto itr = gpuImages_.find(handle); itr != gpuImages_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Image handle not uploaded to GPU");
}

const GpuMaterial& GpuResourceCache::gpuMaterial(assets::MaterialHandle handle) const
{
    if (auto itr = gpuMaterials_.find(handle); itr != gpuMaterials_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Material handle not uploaded to GPU");
}

const GpuMesh& GpuResourceCache::gpuMesh(assets::SubMeshHandle handle) const
{
    if (auto itr = gpuMeshes_.find(handle); itr != gpuMeshes_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Mesh handle not uploaded to GPU");
}

const GpuImage& GpuResourceCache::gpuSkyboxImage(assets::SkyboxHandle handle) const
{
    if (auto itr = gpuSkyboxImages_.find(handle); itr != gpuSkyboxImages_.end())
    {
        return itr->second;
    }

    throw std::runtime_error("Skybox handle not uploaded to GPU");
}

const GpuImage& GpuResourceCache::emptyImage() const
{
    return emptyImage_;
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
    uploadImageData(db);
    uploadMaterialData(db);
    uploadMeshData(db);
    uploadSkyboxImageData(db);
}

void GpuResourceCache::uploadImageData(const assets::AssetDatabase& db)
{
    for (const auto& prefab : db.prefabs())
    {
        for (const auto& image : prefab.second->images())
        {
            const auto imageSize = image.width() * image.height() * 4; //  RGBA8

            auto gpuImage = GpuImage{};
            gpuImage.image = gpuDevice_.createImage(image.width(), image.height());
            gpuImage.memory = gpuDevice_.allocateImageMemory(gpuImage.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

            auto stagingBuffer = gpuDevice_.createStagingBuffer(imageSize);

            auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

            void* data = stagingMemory.mapMemory(0, imageSize);
            std::memcpy(data, image.data().data(), imageSize);
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

            gpuDevice_.copyBufferToImage(*cmd, *stagingBuffer, *gpuImage.image, image.width(), image.height());

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

            gpuImages_.emplace(image.handle(), std::move(gpuImage));
        }
    }
}

void GpuResourceCache::uploadMaterialData(const assets::AssetDatabase& db)
{
    if (db.materialCount() == 0)
    {
        return;
    }

    const auto stride = gpuDevice_.calculateAlignedUboStride(sizeof(GpuMaterialBufferData));

    materialUboBuffer_ = gpuDevice_.createUniformBuffer(stride * db.materialCount());
    materialUboBufferMemory_ = gpuDevice_.allocateStagingBufferMemory(materialUboBuffer_);
    auto mappedMemory = materialUboBufferMemory_.mapMemory(0, VK_WHOLE_SIZE);

    auto currentOffset = uint32_t{0};
    for (const auto& prefab : db.prefabs())
    {
        for (const auto& material : prefab.second->materials())
        {
            auto gpuMaterial = GpuMaterial{};
            gpuMaterial.uboOffset = currentOffset;
            gpuMaterial.diffuseImageHandle = material.diffuseImageHandle();
            gpuMaterials_.emplace(material.handle(), std::move(gpuMaterial));

            auto uboData = GpuMaterialBufferData{};
            uboData.diffuseColor = glm::vec4{material.diffuse(), 1.0f};
            uboData.hasDiffuseTexture = material.diffuseImageHandle() ? 1 : 0;

            std::memcpy(static_cast<std::byte*>(mappedMemory) + currentOffset, &uboData, sizeof(GpuMaterialBufferData));

            currentOffset += static_cast<uint32_t>(stride);
        }
    }

    materialUboBufferMemory_.unmapMemory();
}

void GpuResourceCache::uploadMeshData(const assets::AssetDatabase& db)
{
    auto totalVertices = db.vertexCount();
    auto totalIndices = db.indexCount();

    const auto vertexBufferSize = sizeof(core::Vertex) * totalVertices;

    meshVertexBuffer_ = gpuDevice_.createVertexBuffer(vertexBufferSize);
    meshVertexBufferMemory_ = gpuDevice_.allocateDeviceBufferMemory(meshVertexBuffer_);

    auto vertexStagingBuffer = gpuDevice_.createStagingBuffer(vertexBufferSize);
    auto vertexStagingBufferMemory = gpuDevice_.allocateStagingBufferMemory(vertexStagingBuffer);

    const auto indexBufferSize = sizeof(uint32_t) * totalIndices;
    meshIndexBuffer_ = gpuDevice_.createIndexBuffer(indexBufferSize);
    meshIndexBufferMemory_ = gpuDevice_.allocateDeviceBufferMemory(meshIndexBuffer_);

    auto indexStagingBuffer = gpuDevice_.createStagingBuffer(indexBufferSize);
    auto indexStagingBufferMemory = gpuDevice_.allocateStagingBufferMemory(indexStagingBuffer);

    void* vertexStagingMemory = vertexStagingBufferMemory.mapMemory(0, vertexBufferSize);
    void* indexStagingMemory = indexStagingBufferMemory.mapMemory(0, indexBufferSize);

    auto currentVertexOffset = size_t{0};
    auto currentIndexOffset = size_t{0};
    for (const auto& prefab : db.prefabs())
    {
        for (const auto& mesh : prefab.second->meshes())
        {
            for (const auto& subMesh : mesh.subMeshes())
            {
                auto gpuMesh = GpuMesh{};
                gpuMesh.vertexCount = static_cast<uint32_t>(subMesh.vertices.size());
                gpuMesh.indexCount = static_cast<uint32_t>(subMesh.indices.size());
                gpuMesh.vertexOffset = static_cast<uint32_t>(currentVertexOffset);
                gpuMesh.indexOffset = static_cast<uint32_t>(currentIndexOffset);

                const auto vertexSize = subMesh.vertices.size() * sizeof(core::Vertex);
                const auto indexSize = subMesh.indices.size() * sizeof(uint32_t);

                std::memcpy(static_cast<std::byte*>(vertexStagingMemory) + currentVertexOffset * sizeof(core::Vertex),
                            subMesh.vertices.data(),
                            vertexSize);

                std::memcpy(static_cast<std::byte*>(indexStagingMemory) + currentIndexOffset * sizeof(uint32_t),
                            subMesh.indices.data(),
                            indexSize);

                currentVertexOffset += subMesh.vertices.size();
                currentIndexOffset += subMesh.indices.size();

                gpuMeshes_.emplace(subMesh.handle, std::move(gpuMesh));
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
    for (const auto& skybox : db.skyboxes())
    {
        const auto width = skybox.second->width();
        const auto height = skybox.second->height();
        const auto imageSize = width * height * 4; // RGBA8

        auto gpuImage = GpuImage{};
        gpuImage.image = gpuDevice_.createCubemapImage(width, height);
        gpuImage.memory = gpuDevice_.allocateImageMemory(gpuImage.image, vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto stagingBuffer = gpuDevice_.createStagingBuffer(imageSize * 6);
        auto stagingMemory = gpuDevice_.allocateStagingBufferMemory(stagingBuffer);

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
        void* data = stagingMemory.mapMemory(0, imageSize * 6);
        for (auto face = 0; face < skybox.second->faceCount(); ++face)
        {
            std::memcpy(data + (face * imageSize), skybox.second->imageAt(face)->data().data(), imageSize);
        }
        stagingMemory.unmapMemory();
        gpuDevice_.copyBufferToImage(*cmd, *stagingBuffer, *gpuImage.image, width, height, 6);
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

        gpuSkyboxImages_.emplace(skybox.second->handle(), std::move(gpuImage));
    }
}
} // namespace renderer
