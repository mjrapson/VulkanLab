/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "render_pass_command_info.h"

#include <vulkan/vulkan_raii.hpp>

#include <unordered_map>
#include <vector>

namespace renderer
{
class GpuDevice;

class GeometryPass
{
  public:
    GeometryPass(const GpuDevice& gpuDevice,
                 const vk::Format& surfaceFormat,
                 uint32_t maxFramesInFlight,
                 const std::vector<vk::raii::Buffer>& cameraBuffers);

    void rebuild(const GpuResourceCache& resourceCache);

    void recordCommands(const RenderPassCommandInfo& passInfo);

  private:
    void createDescriptorSetLayouts();
    void createCameraDescriptorPool(uint32_t count);
    void createCameraDescriptorSets(uint32_t count, const std::vector<vk::raii::Buffer>& cameraBuffers);

    void createPipeline(const vk::Format& surfaceFormat);

    void recreateMaterialDescriptorPools(uint32_t count);
    void recreateMaterialDescriptorSets(const GpuResourceCache& resourceCache);

  private:
    const GpuDevice& gpuDevice_;

    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};

    vk::raii::DescriptorSetLayout cameraDescriptorSetLayout_{nullptr};
    vk::raii::DescriptorSetLayout materialDescriptorSetLayout_{nullptr};

    vk::raii::DescriptorPool cameraDescriptorPool_{nullptr};
    vk::raii::DescriptorPool materialDescriptorPool_{nullptr};

    std::vector<vk::raii::DescriptorSet> cameraDescriptorSets_;
    std::unordered_map<uint32_t, vk::raii::DescriptorSet> materialDescriptorSets_;
};
} // namespace renderer
