/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/buffer_object.h"
#include "renderer/descriptor_set_allocator.h"
#include "renderer/draw_command.h"
#include "renderer/gpu_objects.h"

#include <vulkan/vulkan_raii.hpp>

#include <unordered_map>
#include <vector>

namespace renderer
{
class GpuDevice;

class GeometryPass
{
  public:
    explicit GeometryPass(const GpuDevice& gpuDevice);

    void initialize(const vk::Extent2D& extent,
                    const vk::Format& surfaceFormat,
                    uint32_t maxFramesInFlight,
                    std::span<BufferObject> cameraBuffers);

    void resize(const vk::Extent2D& extent);

    void rebuild(const std::unordered_map<MaterialHandle, Material, core::Hash<MaterialHandle>>& materials);

    void recordCommands(uint32_t frameIndex,
                        const vk::raii::CommandBuffer& commandBuffer,
                        const vk::raii::Buffer& vertexBuffer,
                        const vk::raii::Buffer& indexBuffer,
                        const std::unordered_map<MeshHandle, Mesh, core::Hash<MeshHandle>>& meshGpuData,
                        const std::unordered_map<MaterialHandle, Material, core::Hash<MaterialHandle>>& materialGpuData,
                        vk::ImageView colorTargetImageView,
                        std::span<const DrawCommand> drawCommands);

  private:
    void createCameraDescriptorSets(uint32_t count, std::span<BufferObject> cameraBuffers);
    void createPipeline(const vk::Format& surfaceFormat);

  private:
    const GpuDevice& gpuDevice_;

    vk::Extent2D extent_;

    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};

    vk::raii::Image depthImage_{nullptr};
    vk::raii::DeviceMemory depthImageMemory_{nullptr};
    vk::raii::ImageView depthImageView_{nullptr};

    DescriptorSetAllocator cameraDescriptor_;
    DescriptorSetAllocator materialDescriptor_;

    std::vector<vk::raii::DescriptorSet> cameraDescriptorSets_;
    std::unordered_map<MaterialHandle, vk::raii::DescriptorSet, core::Hash<MaterialHandle>> materialDescriptorSets_;
};
} // namespace renderer
