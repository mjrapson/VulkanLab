/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "image.h"

namespace renderer
{
vk::raii::Image createImage(const vk::raii::Device& device, uint32_t width, uint32_t height)
{
    auto imageInfo = vk::ImageCreateInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = vk::Format::eR8G8B8A8Unorm;
    imageInfo.extent = vk::Extent3D{width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    return vk::raii::Image{device, imageInfo};
}

void transitionImageLayout(const vk::Image& image,
                           const vk::raii::CommandBuffer& commandBuffer,
                           vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           vk::AccessFlags2 srcAccessMask,
                           vk::AccessFlags2 dstAccessMask,
                           vk::PipelineStageFlags2 srcStageMask,
                           vk::PipelineStageFlags2 dstStageMask)
{
    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto barrier = vk::ImageMemoryBarrier2{};
    barrier.srcStageMask = srcStageMask;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstStageMask = dstStageMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = subresourceRange;

    auto dependencyInfo = vk::DependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    commandBuffer.pipelineBarrier2(dependencyInfo);
}
} // namespace renderer
