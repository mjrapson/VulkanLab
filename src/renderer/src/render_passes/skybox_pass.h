/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

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
    explicit SkyboxPass(const GpuDevice& gpuDevice);

    void initialize(const vk::Extent2D& extent,
                    const vk::Format& surfaceFormat,
                    uint32_t maxFramesInFlight,
                    const std::vector<vk::raii::Buffer>& cameraBuffers);

    void resize(const vk::Extent2D& extent);

    void rebuild(const std::unordered_map<SkyboxHandle, Skybox, core::Hash<SkyboxHandle>>& skyboxes);

    void recordCommands(uint32_t frameIndex,
                        const vk::raii::CommandBuffer& commandBuffer,
                        SkyboxHandle skyboxHandle,
                        vk::ImageView colorTargetImageView);

  private:
    void createCameraDescriptorSets(uint32_t count, const std::vector<vk::raii::Buffer>& cameraBuffers);
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
