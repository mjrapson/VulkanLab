/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
[[nodiscard]]
uint32_t findMemoryType(const vk::raii::PhysicalDevice& device,
                        uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties);

[[nodiscard]]
vk::raii::DeviceMemory allocateBufferMemory(const vk::raii::Device& device,
                                            const vk::raii::PhysicalDevice& physicalDevice,
                                            const vk::raii::Buffer& buffer,
                                            vk::MemoryPropertyFlags properties);

[[nodiscard]]
vk::raii::DeviceMemory allocateImageMemory(const vk::raii::Device& device,
                                           const vk::raii::PhysicalDevice& physicalDevice,
                                           const vk::raii::Image& image,
                                           vk::MemoryPropertyFlags properties);

[[nodiscard]]
vk::DeviceSize alignMemory(vk::DeviceSize data, vk::DeviceSize alignment);
} // namespace renderer
