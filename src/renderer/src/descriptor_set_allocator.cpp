/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/descriptor_set_allocator.h"

namespace renderer
{
constexpr auto maxSetsPerPool = 64;

DescriptorSetAllocator::DescriptorSetAllocator(const vk::raii::Device& device,
                                               std::span<const vk::DescriptorSetLayoutBinding> bindings)
    : device_{device}
{
    // Create layout
    auto createInfo = vk::DescriptorSetLayoutCreateInfo{};
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();
    layout_ = vk::raii::DescriptorSetLayout{device_, createInfo};

    // Cache bindings
    for (const auto& binding : bindings)
    {
        descriptorCounts_[binding.descriptorType] += binding.descriptorCount;
    }
}

vk::raii::DescriptorSets DescriptorSetAllocator::allocateSets(uint32_t count)
{
    if (pools_.empty())
    {
        expandPools();
    }

    try
    {
        return allocateSets(count, pools_.back());
    }
    catch (std::exception& err)
    {
        expandPools();
        return allocateSets(count, pools_.back());
    }
}

void DescriptorSetAllocator::clear()
{
    pools_.clear();
}

const vk::raii::DescriptorSetLayout& DescriptorSetAllocator::layout() const
{
    return layout_;
}

vk::raii::DescriptorSets DescriptorSetAllocator::allocateSets(uint32_t count,
                                                              const vk::raii::DescriptorPool& pool) const
{
    const auto layouts = std::vector<vk::DescriptorSetLayout>{count, *layout_};

    auto allocateInfo = vk::DescriptorSetAllocateInfo{};
    allocateInfo.descriptorPool = *pool;
    allocateInfo.descriptorSetCount = count;
    allocateInfo.pSetLayouts = layouts.data();

    return vk::raii::DescriptorSets{device_, allocateInfo};
}

void DescriptorSetAllocator::expandPools()
{
    auto poolSizes = std::vector<vk::DescriptorPoolSize>{};
    for (const auto& [type, count] : descriptorCounts_)
    {
        auto poolSize = vk::DescriptorPoolSize{};
        poolSize.type = type;
        poolSize.descriptorCount = count * maxSetsPerPool;
        poolSizes.push_back(poolSize);
    }

    auto createInfo = vk::DescriptorPoolCreateInfo{};
    createInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    createInfo.maxSets = maxSetsPerPool;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();

    pools_.push_back(vk::raii::DescriptorPool{device_, createInfo});
}
} // namespace renderer
