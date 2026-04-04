/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class Image
{
  public:
    enum class ImageType
    {
        Colour2D,
        Depth2D,
        ColourCube,
    };

    Image() = delete;
    Image(VkDevice device,
          VmaAllocator allocator,
          vk::Extent3D extent,
          vk::Format format,
          uint32_t arrayLayers,
          ImageType type);
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other);
    Image& operator=(Image&& other);

    VkImage image() const;
    VkImageView view() const;

  private:
    void cleanup();

  private:
    VkDevice device_{VK_NULL_HANDLE};
    VmaAllocator allocator_{VK_NULL_HANDLE};

    VkImage image_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VkImageView view_{VK_NULL_HANDLE};
};
} // namespace renderer
