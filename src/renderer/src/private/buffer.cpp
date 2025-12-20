/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "buffer.h"

#include "memory.h"

namespace renderer
{
vk::raii::Buffer createBuffer(const vk::raii::Device& device, const vk::DeviceSize& size,
                              const vk::BufferUsageFlags& usage, const vk::SharingMode& sharingMode)
{
    const auto bufferInfo =
        vk::BufferCreateInfo{.size = size, .usage = usage, .sharingMode = sharingMode};

    return vk::raii::Buffer(device, bufferInfo);
}
} // namespace renderer