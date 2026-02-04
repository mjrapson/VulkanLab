/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "render_pass_command_info.h"

#include "renderer/descriptor_set_allocator.h"

#include <assets/handles.h>

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

    void rebuild(const GpuResourceCache& resourceCache);

    void recordCommands(const RenderPassCommandInfo& passInfo, vk::ImageView colorTargetImageView);

  private:
    void createCameraDescriptorSets(uint32_t count, const std::vector<vk::raii::Buffer>& cameraBuffers);

    void createPipeline(const vk::Format& surfaceFormat);
    
    void recreateDescriptorSets(const GpuResourceCache& resourceCache);

  private:
    const GpuDevice& gpuDevice_;

    vk::Extent2D extent_;

    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};

    DescriptorSetAllocator cameraDescriptor_;
    DescriptorSetAllocator skyboxDescriptor_;

    std::vector<vk::raii::DescriptorSet> cameraDescriptorSets_;
    std::unordered_map<assets::SkyboxHandle, vk::raii::DescriptorSet> skyboxDescriptorSets_;
};
} // namespace renderer
