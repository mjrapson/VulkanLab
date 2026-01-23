/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/draw_command.h"

#include <vulkan/vulkan_raii.hpp>

#include <span>
#include <vector>

namespace renderer
{
class GpuResourceCache;

struct RenderPassCommandInfo
{
    uint32_t frameIndex;
    const vk::Image& colorImage;
    const vk::ImageView& colorImageView;
    const vk::Image& depthImage;
    const vk::ImageView& depthImageView;
    const vk::Extent2D& extent;
    const vk::raii::CommandBuffer& commandBuffer;
    const vk::raii::DescriptorSet& cameraDescriptorSet;
    GpuResourceCache& gpuResourceCache;
    std::span<const DrawCommand> drawCommands;
};

class GeometryPass
{
  public:
    GeometryPass(const vk::raii::Device& device,
                 const vk::raii::PhysicalDevice& physicalDevice,
                 const vk::Format& surfaceFormat,
                 const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout,
                 const vk::raii::DescriptorSetLayout& materialDescriptorSetLayout);

    void recordCommands(const RenderPassCommandInfo& passInfo);

  private:
    void createPipeline(const vk::raii::Device& device,
                        const vk::raii::PhysicalDevice& physicalDevice,
                        const vk::Format& surfaceFormat,
                        const vk::raii::DescriptorSetLayout& cameraDescriptorSetLayout,
                        const vk::raii::DescriptorSetLayout& materialDescriptorSetLayout);

  private:
    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};
} // namespace renderer
