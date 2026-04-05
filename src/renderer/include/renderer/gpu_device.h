// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/buffer.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>

#include <filesystem>

namespace renderer
{
class GpuDevice
{
  public:
    GpuDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);
    ~GpuDevice();

    GpuDevice(const GpuDevice&) = delete;
    GpuDevice& operator=(const GpuDevice&) = delete;

    GpuDevice(GpuDevice&& other) = delete;
    GpuDevice& operator=(GpuDevice&& other) = delete;

    vk::raii::CommandPool createCommandPool() const;

    vk::raii::CommandBuffers createCommandBuffers(const vk::raii::CommandPool& pool, uint32_t count) const;
    void submitCommandBuffer(const vk::CommandBuffer& cmd) const;
    void submitCommandBuffer(const vk::CommandBuffer& cmd,
                             std::span<vk::Semaphore> waitSemaphores,
                             const vk::PipelineStageFlags& waitStageMask,
                             std::span<vk::Semaphore> signalSemaphores,
                             const vk::Fence& fence) const;

    vk::raii::ShaderModule createShaderModule(const std::filesystem::path& filePath) const;

    vk::DeviceSize calculateAlignedUboStride(size_t uboSize) const;

    vk::raii::Device& device();
    const vk::raii::PhysicalDevice& physicalDevice() const;
    const vk::raii::Queue& presentQueue() const;
    VmaAllocator allocator() const;

  private:
    void pickPhysicalDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);
    void createLogicalDevice(const vk::raii::SurfaceKHR& surface);

    bool isDeviceSuitable(vk::raii::PhysicalDevice device, const vk::raii::SurfaceKHR& surface) const;
    vk::raii::PhysicalDevice selectBestDevice(const std::vector<vk::raii::PhysicalDevice>& devices) const;

  private:
    vk::raii::Device device_{nullptr};
    vk::raii::PhysicalDevice physicalDevice_{nullptr};
    vk::raii::Queue graphicsQueue_{nullptr};
    vk::raii::Queue presentQueue_{nullptr};
    vk::raii::Queue computeQueue_{nullptr};
    uint32_t graphicsQueueFamilyIndex_;
    uint32_t presentQueueFamilyIndex_;
    uint32_t computeQueueFamilyIndex_;
    VmaAllocator allocator_{nullptr};
};
} // namespace renderer
