/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
[[nodiscard]]
uint32_t findMemoryType(const vk::raii::PhysicalDevice& device, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties);

vk::raii::DeviceMemory allocateBufferMemory(const vk::raii::Device& device,
                                            const vk::raii::PhysicalDevice& physicalDevice,
                                            const vk::raii::Buffer& buffer,
                                            vk::MemoryPropertyFlags properties);
} // namespace renderer