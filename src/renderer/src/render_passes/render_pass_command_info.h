/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <optional>
#include <span>

namespace assets
{
struct Skybox;
}

namespace renderer
{
struct DrawCommand;
class GpuResourceCache;

struct RenderPassCommandInfo
{
    uint32_t frameIndex;
    const vk::Image& colorImage;
    const vk::ImageView& colorImageView;
    const vk::Image& depthImage;
    const vk::ImageView& depthImageView;
    const vk::Extent2D& extent;
    const vk::raii::CommandBuffer& commandBuffer;
    const vk::raii::DescriptorSet& cameraDescriptorSet;
    assets::Skybox* skybox{nullptr};
    GpuResourceCache& gpuResourceCache;
    std::span<const DrawCommand> drawCommands;
};
} // namespace renderer
