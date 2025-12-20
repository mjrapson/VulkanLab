/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
vk::raii::Buffer createBuffer(const vk::raii::Device& device, const vk::DeviceSize& size,
                              const vk::BufferUsageFlags& usage,
                              const vk::SharingMode& sharingMode);
}
