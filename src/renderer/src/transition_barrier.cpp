/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/transition_barrier.h"

namespace renderer
{
vk::PipelineStageFlags2 requiredStageMaskForLayout(vk::ImageLayout layout)
{
    switch (layout)
    {
        case vk::ImageLayout::eUndefined:
            return vk::PipelineStageFlagBits2::eTopOfPipe;
        case vk::ImageLayout::eColorAttachmentOptimal:
            return vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        case vk::ImageLayout::eDepthAttachmentOptimal:
            return vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
        case vk::ImageLayout::eTransferDstOptimal:
            return vk::PipelineStageFlagBits2::eTransfer;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader
                   | vk::PipelineStageFlagBits2::ePreRasterizationShaders | vk::PipelineStageFlagBits2::eAllCommands;
        case vk::ImageLayout::ePresentSrcKHR:
            return vk::PipelineStageFlagBits2::eBottomOfPipe;
        default:
            assert(false && "Unhandled image layout - defaulting to undefined");
            return {};
    }
}

vk::AccessFlags2 requiredAccessMaskForLayout(vk::ImageLayout layout)
{
    switch (layout)
    {
        case vk::ImageLayout::eUndefined:
            return vk::AccessFlagBits2::eNone;
        case vk::ImageLayout::eColorAttachmentOptimal:
            return vk::AccessFlagBits2::eColorAttachmentWrite;
        case vk::ImageLayout::eDepthAttachmentOptimal:
            return vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
        case vk::ImageLayout::eTransferDstOptimal:
            return vk::AccessFlagBits2::eTransferWrite;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return vk::AccessFlagBits2::eShaderRead;
        case vk::ImageLayout::ePresentSrcKHR:
            return vk::AccessFlagBits2::eNone;
        default:
            assert(false && "Unhandled image layout - defaulting to undefined");
            return vk::AccessFlagBits2::eNone;
    }
}

void transitionImageLayout(const vk::Image& image,
                           const vk::CommandBuffer& commandBuffer,
                           vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           const vk::ImageAspectFlags& aspectFlags,
                           uint32_t layerCount)
{
    auto barrier = vk::ImageMemoryBarrier2{};
    barrier.srcStageMask = requiredStageMaskForLayout(oldLayout);
    barrier.dstStageMask = requiredStageMaskForLayout(newLayout);
    barrier.srcAccessMask = requiredAccessMaskForLayout(oldLayout);
    barrier.dstAccessMask = requiredAccessMaskForLayout(newLayout);
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    auto dependencyInfo = vk::DependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    commandBuffer.pipelineBarrier2(dependencyInfo);
}
} // namespace renderer
