/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/descriptor_set_allocator.h"

#include "renderer/gpu_device.h"

namespace renderer
{
DescriptorSetAllocator::DescriptorSetAllocator(const GpuDevice& device,
                                               std::span<const vk::DescriptorSetLayoutBinding> bindings)
    : gpuDevice_{device}
{
    layout_ = gpuDevice_.createDescriptorSetLayout(bindings);

    for (const auto& binding : bindings)
    {
        descriptorCounts_[binding.descriptorType] += binding.descriptorCount;
    }
}

void DescriptorSetAllocator::resize(uint32_t maxSets)
{
    auto poolSizes = std::vector<vk::DescriptorPoolSize>{};
    for (const auto& [type, count] : descriptorCounts_)
    {
        auto poolSize = vk::DescriptorPoolSize{};
        poolSize.type = type;
        poolSize.descriptorCount = count * maxSets;
        poolSizes.push_back(poolSize);
    }

    pool_ = gpuDevice_.createDescriptorPool(maxSets, poolSizes);
}

vk::raii::DescriptorSets DescriptorSetAllocator::allocateSets(uint32_t count) const
{
    return gpuDevice_.createDescriptorSets(layout_, pool_, count);
}

const vk::raii::DescriptorSetLayout& DescriptorSetAllocator::layout() const
{
    return layout_;
}
} // namespace renderer
