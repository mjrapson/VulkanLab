/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class Buffer
{
  public:
    enum class BufferType
    {
        Vertex,
        Index,
        Staging,
        Uniform
    };

    Buffer() = delete;
    Buffer(VkDevice device, VmaAllocator allocator, vk::DeviceSize size, BufferType type);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other);
    Buffer& operator=(Buffer&& other);

    VkBuffer buffer() const;

    bool isHostVisible() const;

    void write(const void* data, size_t offset, size_t size);

  private:
    void cleanup();

  private:
    VkDevice device_{VK_NULL_HANDLE};
    VmaAllocator allocator_{VK_NULL_HANDLE};

    VkBuffer buffer_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    void* mapped_{nullptr};
};
} // namespace renderer
