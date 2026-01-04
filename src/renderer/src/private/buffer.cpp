/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "buffer.h"

#include "image.h"
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

void copyBufferToImage(const vk::raii::Device& device,
                       const vk::raii::Buffer& source,
                       const vk::raii::Image& destination,
                       const vk::raii::Queue& graphicsQueue,
                       const vk::raii::CommandPool& commandPool,
                       uint32_t width,
                       uint32_t height)
{
    auto allocInfo = vk::CommandBufferAllocateInfo{};
    allocInfo.commandPool = *commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    auto commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    auto& cmd = commandBuffers[0];

    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    transitionImageLayout(*destination,
                          cmd,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal,
                          {}, // srcAccess
                          vk::AccessFlagBits2::eTransferWrite,
                          vk::PipelineStageFlagBits2::eTopOfPipe,
                          vk::PipelineStageFlagBits2::eTransfer);

    auto region = vk::BufferImageCopy{};
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = vk::Extent3D{width, height, 1};

    cmd.copyBufferToImage(*source, *destination, vk::ImageLayout::eTransferDstOptimal, region);

    transitionImageLayout(*destination,
                          cmd,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::AccessFlagBits2::eTransferWrite,
                          vk::AccessFlagBits2::eShaderRead,
                          vk::PipelineStageFlagBits2::eTransfer,
                          vk::PipelineStageFlagBits2::eFragmentShader);

    cmd.end();

    auto submitInfo = vk::SubmitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*cmd;

    graphicsQueue.submit(submitInfo);
    graphicsQueue.waitIdle();
}
} // namespace renderer
