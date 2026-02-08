/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <span>
#include <unordered_map>

namespace renderer
{
class GpuDevice;

class DescriptorSetAllocator
{
  public:
    DescriptorSetAllocator(const GpuDevice& device, std::span<const vk::DescriptorSetLayoutBinding> bindings);

    void resize(uint32_t maxSets);
    vk::raii::DescriptorSets allocateSets(uint32_t count) const;

    const vk::raii::DescriptorSetLayout& layout() const;

    uint32_t size() const;

  private:
    const GpuDevice& gpuDevice_;
    std::unordered_map<vk::DescriptorType, uint32_t> descriptorCounts_;
    vk::raii::DescriptorSetLayout layout_{nullptr};
    vk::raii::DescriptorPool pool_{nullptr};
    uint32_t maxSets_{0};
};
} // namespace renderer
