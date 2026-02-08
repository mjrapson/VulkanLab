/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/descriptor_set_allocator.h"

#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace renderer
{
class GpuDevice;
struct GpuImage;

class LoadingScreenPass
{
  public:
    explicit LoadingScreenPass(const GpuDevice& gpuDevice);

    void initialize(const vk::Extent2D& extent, const vk::Format& surfaceFormat, uint32_t maxFramesInFlight);

    void resize(const vk::Extent2D& extent);

    void setImage(std::unique_ptr<GpuImage> image);

    void recordCommands(uint32_t frameIndex,
                        const vk::raii::CommandBuffer& commandBuffer,
                        vk::ImageView colorTargetImageView);

  private:
    void createImageDescriptorSet();

    void createPipeline(const vk::Format& surfaceFormat);

  private:
    const GpuDevice& gpuDevice_;
    std::unique_ptr<GpuImage> loadingScreenImage_{nullptr};
    vk::Extent2D extent_;

    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};

    DescriptorSetAllocator imageDescriptor_;
    std::vector<vk::raii::DescriptorSet> imageDescriptorSets_;
};
} // namespace renderer
