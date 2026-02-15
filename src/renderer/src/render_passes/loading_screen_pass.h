/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/descriptor_set_allocator.h"
#include "renderer/gpu_objects.h"
#include "renderer/handles.h"

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class GpuDevice;
struct Image;

class LoadingScreenPass
{
  public:
    LoadingScreenPass(const GpuDevice& gpuDevice, const vk::Format& surfaceFormat, const vk::Extent2D& extent);

    void regenerateDescriptorSets(const ImageContainer& loadingScreenImages);

    void resize(const vk::Extent2D& extent);

    void recordCommands(const vk::raii::CommandBuffer& commandBuffer,
                        ImageHandle loadingScreenHandle,
                        vk::ImageView colorTargetImageView);

  private:
    void createPipeline(const vk::Format& surfaceFormat);

  private:
    const GpuDevice& gpuDevice_;
    vk::Extent2D extent_;

    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};

    DescriptorSetAllocator imageDescriptor_;
    std::unordered_map<ImageHandle, vk::raii::DescriptorSet, core::Hash<ImageHandle>> imageDescriptorSets_;
};
} // namespace renderer
