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
    const auto bufferInfo = vk::BufferCreateInfo{{}, size, usage, sharingMode};

    return vk::raii::Buffer(device, bufferInfo);
}

void copyBuffer(const vk::raii::Device& device,
                const vk::raii::Buffer& source,
                const vk::raii::Buffer& destination,
                const vk::raii::Queue& graphicsQueue,
                const vk::raii::CommandPool& commandPool,
                const vk::DeviceSize& size)
{
    auto allocInfo = vk::CommandBufferAllocateInfo{};
    allocInfo.commandPool = *commandPool;
    allocInfo.commandBufferCount = 1;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;

    auto commandCopyBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

    auto commandBufferBeginInfo = vk::CommandBufferBeginInfo{};
    commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    commandCopyBuffer.begin(commandBufferBeginInfo);
    commandCopyBuffer.copyBuffer(*source, *destination, vk::BufferCopy(0, 0, size));
    commandCopyBuffer.end();

    auto graphicsSubmitInfo = vk::SubmitInfo{};
    graphicsSubmitInfo.commandBufferCount = 1;
    graphicsSubmitInfo.pCommandBuffers = &*commandCopyBuffer;
    graphicsQueue.submit(graphicsSubmitInfo, nullptr);
    graphicsQueue.waitIdle();
}
} // namespace renderer