/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/image.h"

namespace renderer
{
vk::ImageUsageFlags usageFlagsForType(Image::ImageType type)
{
    switch (type)
    {
        case Image::ImageType::Colour2D:
        case Image::ImageType::ColourCube:
            return vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        case Image::ImageType::Depth2D:
            return vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled
                   | vk::ImageUsageFlagBits::eDepthStencilAttachment;
        default:
            return {};
    }
}

vk::ImageAspectFlags aspectFlagsForType(Image::ImageType type)
{
    switch (type)
    {
        case Image::ImageType::Colour2D:
        case Image::ImageType::ColourCube:
            return vk::ImageAspectFlagBits::eColor;
        case Image::ImageType::Depth2D:
            return vk::ImageAspectFlagBits::eDepth;
        default:
            return {};
    }
}

vk::ImageCreateFlags imageCreateFlagsForType(Image::ImageType type)
{
    switch (type)
    {
        case Image::ImageType::ColourCube:
            return vk::ImageCreateFlagBits::eCubeCompatible;
        case Image::ImageType::Colour2D:
        case Image::ImageType::Depth2D:
        default:
            return {};
    }
}

vk::ImageViewType viewTypeForType(Image::ImageType type)
{
    switch (type)
    {
        case Image::ImageType::ColourCube:
            return vk::ImageViewType::eCube;
        case Image::ImageType::Colour2D:
        case Image::ImageType::Depth2D:
            return vk::ImageViewType::e2D;
        default:
            return {};
    }
}

Image::Image(VkDevice device,
             VmaAllocator allocator,
             vk::Extent3D extent,
             vk::Format format,
             uint32_t arrayLayers,
             ImageType type)
    : device_{device},
      allocator_(allocator)
{
    const auto usage = usageFlagsForType(type);
    const auto aspect = aspectFlagsForType(type);
    const auto createFlags = imageCreateFlagsForType(type);
    const auto viewType = viewTypeForType(type);

    auto imageInfo = vk::ImageCreateInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format;
    imageInfo.extent = extent;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.flags = createFlags;

    auto createInfo = VmaAllocationCreateInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(allocator_, &*imageInfo, &createInfo, &image_, &allocation_, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image");
    }

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.image = image_;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.aspectMask = aspect;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = arrayLayers;

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
