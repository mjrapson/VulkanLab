/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/image.h"

namespace renderer
{
bool isDepthFormat(vk::Format format)
{
    static constexpr auto depthFormats = std::array{vk::Format::eD16Unorm,
                                                    vk::Format::eD32Sfloat,
                                                    vk::Format::eD16UnormS8Uint,
                                                    vk::Format::eD24UnormS8Uint,
                                                    vk::Format::eD32SfloatS8Uint};

    return std::find(depthFormats.begin(), depthFormats.end(), format) != depthFormats.end();
}

Image::Image(VkDevice device, VmaAllocator allocator, vk::Extent3D extent, vk::Format format)
    : device_{device},
      allocator_(allocator)
{
    auto usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    if (isDepthFormat(format))
    {
        usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    }

    auto aspect = isDepthFormat(format) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;

    auto imageInfo = vk::ImageCreateInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format;
    imageInfo.extent = extent;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    auto createInfo = VmaAllocationCreateInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(allocator_, &*imageInfo, &createInfo, &image_, &allocation_, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image");
    }

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.image = image_;
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.aspectMask = aspect;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device_, imageViewCreateInfo, nullptr, &view_) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image view");
    }
}

Image::~Image()
{
    cleanup();
}

Image::Image(Image&& other)
    : device_(other.device_),
      allocator_(other.allocator_),
      image_(other.image_),
      allocation_(other.allocation_),
      view_(other.view_)
{
    other.image_ = VK_NULL_HANDLE;
    other.allocation_ = VK_NULL_HANDLE;
    other.view_ = VK_NULL_HANDLE;
}

Image& Image::operator=(Image&& other)
{
    if (this != &other)
    {
        cleanup();

        device_ = other.device_;
        allocator_ = other.allocator_;
        image_ = other.image_;
        allocation_ = other.allocation_;
        view_ = other.view_;

        other.image_ = VK_NULL_HANDLE;
        other.allocation_ = VK_NULL_HANDLE;
        other.view_ = VK_NULL_HANDLE;
    }
    return *this;
}

VkImage Image::image() const
{
    return image_;
}

VkImageView Image::view() const
{
    return view_;
}

void Image::cleanup()
{
    if (view_ != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device_, view_, nullptr);
        view_ = VK_NULL_HANDLE;
    }

    if (image_ != VK_NULL_HANDLE)
    {
        vmaDestroyImage(allocator_, image_, allocation_);
        image_ = VK_NULL_HANDLE;
        allocation_ = nullptr;
    }
}
} // namespace renderer
