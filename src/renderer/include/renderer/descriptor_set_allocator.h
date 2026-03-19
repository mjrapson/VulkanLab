/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <span>
#include <unordered_map>

namespace renderer
{
class DescriptorSetAllocator
{
  public:
    DescriptorSetAllocator(const vk::raii::Device& device, std::span<const vk::DescriptorSetLayoutBinding> bindings);

    vk::raii::DescriptorSets allocateSets(uint32_t count);

    void clear();

    const vk::raii::DescriptorSetLayout& layout() const;

  private:
    vk::raii::DescriptorSets allocateSets(uint32_t count, const vk::raii::DescriptorPool& pool) const;
    void expandPools();

  private:
    const vk::raii::Device& device_;
    std::unordered_map<vk::DescriptorType, uint32_t> descriptorCounts_;
    vk::raii::DescriptorSetLayout layout_{nullptr};
    std::vector<vk::raii::DescriptorPool> pools_;
    uint32_t maxSets_{0};
};
} // namespace renderer
