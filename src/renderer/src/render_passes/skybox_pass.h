/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "render_pass_command_info.h"

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class SkyboxPass
{
  public:
    SkyboxPass(const vk::raii::Device& device,
               const vk::Format& surfaceFormat,
               const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout);

    void recordCommands(const RenderPassCommandInfo& passInfo);

  private:
    void createPipeline(const vk::raii::Device& device,
                        const vk::Format& surfaceFormat,
                        const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout);

  private:
    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};
} // namespace renderer
