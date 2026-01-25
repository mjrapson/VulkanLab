/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "render_pass_command_info.h"

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class GpuDevice;

class SkyboxPass
{
  public:
    SkyboxPass(const GpuDevice& gpuDevice,
               const vk::Format& surfaceFormat,
               const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout,
               const vk::raii::DescriptorSetLayout& skyboxDescriptorSetLayout);

    void recordCommands(const RenderPassCommandInfo& passInfo);

  private:
    void createPipeline(const vk::Format& surfaceFormat,
                        const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout,
                        const vk::raii::DescriptorSetLayout& skyboxDescriptorSetLayout);

  private:
    const GpuDevice& gpuDevice_;
    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};
} // namespace renderer
