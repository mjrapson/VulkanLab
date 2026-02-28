/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/descriptor_set_allocator.h"
#include "renderer/draw_command.h"

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class GpuDevice;
struct Buffers;
struct Resources;

class ShadowMapPass
{
  public:
    explicit ShadowMapPass(const GpuDevice& gpuDevice);

    void regenerateDescriptorSets(const Buffers& buffers);

    void recordCommands(const vk::raii::CommandBuffer& commandBuffer,
                        const Buffers& buffers,
                        const Resources& resources,
                        std::span<const DrawCommand> drawCommands);

  private:
    void createPipeline();

  private:
    const GpuDevice& gpuDevice_;

    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};

    vk::raii::Image depthImage_{nullptr};
    vk::raii::DeviceMemory depthImageMemory_{nullptr};
    vk::raii::ImageView depthImageView_{nullptr};

    DescriptorSetAllocator directionalLightDescriptor_;
    vk::raii::DescriptorSet directionalLightDescriptorSet_{nullptr};
};
} // namespace renderer
