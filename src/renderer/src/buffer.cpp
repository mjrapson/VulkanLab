/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/buffer.h"

namespace renderer
{
vk::BufferUsageFlags usageForType(Buffer::BufferType type)
{
    switch (type)
    {
        case Buffer::BufferType::Vertex:
            return vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        case Buffer::BufferType::Index:
            return vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        case Buffer::BufferType::Staging:
            return vk::BufferUsageFlagBits::eTransferSrc;
        case Buffer::BufferType::Uniform:
            return vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
        default:
            return {};
    }
}

VmaAllocationCreateFlags hostFlagsForType(Buffer::BufferType type)
{
    switch (type)
    {
        case Buffer::BufferType::Vertex:
        case Buffer::BufferType::Index:
            return {};
        case Buffer::BufferType::Staging:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        case Buffer::BufferType::Uniform:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        default:
            return {};
    }
}

Buffer::Buffer(VkDevice device, VmaAllocator allocator, vk::DeviceSize size, BufferType type)
    : device_{device},
      allocator_{allocator}
{

    const auto usageFlags = usageForType(type);
    const auto hostFlags = hostFlagsForType(type);

    auto bufferInfo = vk::BufferCreateInfo{};
    bufferInfo.size = size;
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    auto createInfo = VmaAllocationCreateInfo{};
    createInfo.usage = VMA_MEMORY_USAGE_AUTO;
    createInfo.flags = hostFlags;

    auto allocInfo = VmaAllocationInfo{};

    if (vmaCreateBuffer(allocator_, &*bufferInfo, &createInfo, &buffer_, &allocation_, &allocInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create buffer");
    }

    mapped_ = allocInfo.pMappedData;
}

Buffer::~Buffer()
{
    cleanup();
}

Buffer::Buffer(Buffer&& other)
    : device_(other.device_),
      allocator_(other.allocator_),
      buffer_(other.buffer_),
      allocation_(other.allocation_),
      mapped_(other.mapped_)
{
    other.buffer_ = VK_NULL_HANDLE;
    other.allocation_ = VK_NULL_HANDLE;
    other.mapped_ = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other)
{
    if (this != &other)
    {
        cleanup();

        device_ = other.device_;
        allocator_ = other.allocator_;
        buffer_ = other.buffer_;
        allocation_ = other.allocation_;
        mapped_ = other.mapped_;

        other.buffer_ = VK_NULL_HANDLE;
        other.allocation_ = VK_NULL_HANDLE;
        other.mapped_ = nullptr;
    }
    return *this;
}

VkBuffer Buffer::buffer() const
{
    return buffer_;
}

void Buffer::cleanup()
{
    if (buffer_ != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
    }
}

bool Buffer::isHostVisible() const
{
    return mapped_ != nullptr;
}

void Buffer::write(const void* data, size_t offset, size_t size)
{
    if (!isHostVisible())
    {
        assert(false
               && "Cannot write to non-host visible buffer - use staging buffer upload instead (e.g. "
                  "GpuDevice::copyBuffer)");
        return;
    }

    std::memcpy(static_cast<std::byte*>(mapped_) + offset, data, size);
    vmaFlushAllocation(allocator_, allocation_, offset, size);
}
} // namespace renderer
