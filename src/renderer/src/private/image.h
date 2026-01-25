/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
[[nodiscard]]
vk::raii::Image createImage(const vk::raii::Device& device, uint32_t width, uint32_t height);

[[nodiscard]]
vk::raii::Image createCubemapImage(const vk::raii::Device& device, uint32_t width, uint32_t height);

[[nodiscard]]
vk::raii::Image createDepthImage(const vk::raii::Device& device, uint32_t width, uint32_t height);

[[nodiscard]]
vk::raii::ImageView createImageView(const vk::raii::Device& device,
                                    const vk::raii::Image& image,
                                    const vk::Format& format,
                                    const vk::ImageAspectFlags& aspectFlags);

[[nodiscard]]
vk::raii::ImageView createImageCubemapView(const vk::raii::Device& device,
                                           const vk::raii::Image& image,
                                           const vk::Format& format,
                                           const vk::ImageAspectFlags& aspectFlags);

void transitionImageLayout(const vk::Image& image,
                           const vk::raii::CommandBuffer& commandBuffer,
                           vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           vk::AccessFlags2 srcAccessMask,
                           vk::AccessFlags2 dstAccessMask,
                           vk::PipelineStageFlags2 srcStageMask,
                           vk::PipelineStageFlags2 dstStageMask,
                           const vk::ImageAspectFlags& aspectFlags);
} // namespace renderer
