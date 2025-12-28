/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "memory.h"

#include <stdexcept>

namespace renderer
{
[[nodiscard]]
bool matchingMemoryIndex(uint32_t memoryIndex, uint32_t filter)
{
    return (filter & (1 << memoryIndex));
}

[[nodiscard]]
uint32_t findMemoryType(const vk::raii::PhysicalDevice& device,
                        uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties)
{

    const auto memoryProperties = device.getMemoryProperties();
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if (!matchingMemoryIndex(i, typeFilter))
        {
            continue;
        }

        if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("No suitable memory type found");
}

vk::raii::DeviceMemory allocateBufferMemory(const vk::raii::Device& device,
                                            const vk::raii::PhysicalDevice& physicalDevice,
                                            const vk::raii::Buffer& buffer,
                                            vk::MemoryPropertyFlags properties)
{
    const auto memoryRequirements = buffer.getMemoryRequirements();
    const auto memoryAllocateInfo = vk::MemoryAllocateInfo{
        memoryRequirements.size,
        findMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties)};

    auto memory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
    buffer.bindMemory(*memory, 0);

    return memory;
}
} // namespace renderer