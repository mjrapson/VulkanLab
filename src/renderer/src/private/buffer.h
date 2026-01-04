/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
[[nodiscard]]
vk::raii::Buffer createBuffer(const vk::raii::Device& device,
                              const vk::DeviceSize& size,
                              const vk::BufferUsageFlags& usage,
                              const vk::SharingMode& sharingMode);

void copyBuffer(const vk::raii::Device& device,
                const vk::raii::Buffer& source,
                const vk::raii::Buffer& destination,
                const vk::raii::Queue& graphicsQueue,
                const vk::raii::CommandPool& commandPool,
                const vk::DeviceSize& size);

void copyBufferToImage(const vk::raii::Device& device,
                       const vk::raii::Buffer& source,
                       const vk::raii::Image& destination,
                       const vk::raii::Queue& graphicsQueue,
                       const vk::raii::CommandPool& commandPool,
                       uint32_t width,
                       uint32_t height);
} // namespace renderer
