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
    GeometryPass(const GpuDevice& gpuDevice, const vk::Format& surfaceFormat, const vk::Extent2D& extent);

    void regenerateDescriptorSets(std::span<BufferObject> cameraBuffers,
                                  const MaterialContainer& materials,
                                  const DirectionalLight& directionalLight);

    void resize(const vk::Extent2D& extent);

    void recordCommands(uint32_t frameIndex,
                        const vk::raii::CommandBuffer& commandBuffer,
                        const vk::raii::Buffer& vertexBuffer,
                        const vk::raii::Buffer& indexBuffer,
                        const MeshContainer& meshGpuData,
                        const MaterialContainer& materialGpuData,
                        vk::ImageView colorTargetImageView,
                        std::span<const DrawCommand> drawCommands);

  private:
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
    DescriptorSetAllocator directionalLightDescriptor_;

    std::vector<vk::raii::DescriptorSet> cameraDescriptorSets_;
    std::unordered_map<MaterialHandle, vk::raii::DescriptorSet, core::Hash<MaterialHandle>> materialDescriptorSets_;
    vk::raii::DescriptorSet directionalLightDescriptorSet_{nullptr};
};
} // namespace renderer
