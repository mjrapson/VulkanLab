/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/buffer_object.h"
#include "renderer/descriptor_set_allocator.h"
#include "renderer/gpu_objects.h"
#include "renderer/handles.h"

#include <vulkan/vulkan_raii.hpp>

#include <unordered_map>
#include <vector>

namespace renderer
{
class GpuDevice;

class SkyboxPass
{
  public:
    SkyboxPass(const GpuDevice& gpuDevice, const vk::Format& surfaceFormat, const vk::Extent2D& extent);

    void regenerateDescriptorSets(std::span<BufferObject> cameraBuffers, const SkyboxContainer& skyboxes);

    void resize(const vk::Extent2D& extent);

    void recordCommands(uint32_t frameIndex,
                        const vk::raii::CommandBuffer& commandBuffer,
                        SkyboxHandle skyboxHandle,
                        vk::ImageView colorTargetImageView);

  private:
    void createPipeline(const vk::Format& surfaceFormat);

  private:
    const GpuDevice& gpuDevice_;

    vk::Extent2D extent_;

    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};

    DescriptorSetAllocator cameraDescriptor_;
    DescriptorSetAllocator skyboxDescriptor_;

    std::vector<vk::raii::DescriptorSet> cameraDescriptorSets_;
    std::unordered_map<SkyboxHandle, vk::raii::DescriptorSet, core::Hash<SkyboxHandle>> skyboxDescriptorSets_;
};
} // namespace renderer
