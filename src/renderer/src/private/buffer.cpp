/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "buffer.h"

#include "memory.h"

namespace renderer
{
vk::raii::Buffer createBuffer(const vk::raii::Device& device,
                              const vk::DeviceSize& size,
                              const vk::BufferUsageFlags& usage,
                              const vk::SharingMode& sharingMode)
{
    const auto bufferInfo = vk::BufferCreateInfo{
        .size = size,
        .usage = usage,
        .sharingMode = sharingMode,
    };

    return vk::raii::Buffer(device, bufferInfo);
}

void copyBuffer(const vk::raii::Device& device,
                const vk::raii::Buffer& source,
                const vk::raii::Buffer& destination,
                const vk::raii::Queue& graphicsQueue,
                const vk::raii::CommandPool& commandPool,
                const vk::DeviceSize& size)
{
    vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                            .level = vk::CommandBufferLevel::ePrimary,
                                            .commandBufferCount = 1};
    vk::raii::CommandBuffer commandCopyBuffer =
        std::move(device.allocateCommandBuffers(allocInfo).front());

    commandCopyBuffer.begin(
        vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    commandCopyBuffer.copyBuffer(source, destination, vk::BufferCopy(0, 0, size));
    commandCopyBuffer.end();
    graphicsQueue.submit(
        vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer}, nullptr);
    graphicsQueue.waitIdle();
}
} // namespace renderer