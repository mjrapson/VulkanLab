/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
void transitionImageLayout(vk::Image image,
                           const vk::CommandBuffer& commandBuffer,
                           vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           const vk::ImageAspectFlags& aspectFlags,
                           uint32_t layerCount = 1);
}
