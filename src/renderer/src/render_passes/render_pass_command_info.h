/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <assets/handles.h>

#include <vulkan/vulkan_raii.hpp>

#include <optional>
#include <span>

namespace renderer
{
struct DrawCommand;
class GpuResourceCache;

struct RenderPassCommandInfo
{
    uint32_t frameIndex;
    const vk::raii::CommandBuffer& commandBuffer;
    std::optional<assets::SkyboxHandle> skyboxHandle;
    GpuResourceCache& gpuResourceCache;
    std::span<const DrawCommand> drawCommands;
};
} // namespace renderer
