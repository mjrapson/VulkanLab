// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <span>

namespace renderer
{
class GpuDevice
{
  public:
    GpuDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);

    GpuDevice(const GpuDevice&) = delete;
    GpuDevice& operator=(const GpuDevice&) = delete;

    GpuDevice(GpuDevice&& other) = delete;
    GpuDevice& operator=(GpuDevice&& other) = delete;

    vk::raii::CommandBuffers createCommandBuffers(uint32_t count) const;
    void submitCommandBuffer(const vk::CommandBuffer& cmd) const;
    void submitCommandBuffer(const vk::CommandBuffer& cmd,
                             std::span<vk::Semaphore> waitSemaphores,
                             const vk::PipelineStageFlags& waitStageMask,
                             std::span<vk::Semaphore> signalSemaphores,
                             const vk::Fence& fence) const;

    vk::raii::Buffer createBuffer(const vk::DeviceSize& size,
                                  const vk::BufferUsageFlags& usage,
                                  const vk::SharingMode& sharingMode) const;

    void copyBuffer(const vk::raii::Buffer& source,
                    const vk::raii::Buffer& destination,
                    const vk::DeviceSize& size) const;
    void copyBufferToImage(const vk::CommandBuffer& cmd,
                           const vk::Buffer& source,
                           const vk::Image& destination,
                           uint32_t width,
                           uint32_t height,
                           uint32_t baseArrayLayer = 0) const;

    vk::raii::Image createImage(uint32_t width, uint32_t height) const;
    vk::raii::Image createCubemapImage(uint32_t width, uint32_t height) const;
    vk::raii::Image createDepthImage(uint32_t width, uint32_t height) const;

    vk::raii::ImageView createImageView(const vk::raii::Image& image) const;
    vk::raii::ImageView createDepthImageView(const vk::raii::Image& image) const;
    vk::raii::ImageView createCubemapImageView(const vk::raii::Image& image) const;

    vk::raii::Sampler createSampler() const;

    void transitionImageLayout(const vk::Image& image,
                               const vk::CommandBuffer& commandBuffer,
                               vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout,
                               vk::AccessFlags2 srcAccessMask,
                               vk::AccessFlags2 dstAccessMask,
                               vk::PipelineStageFlags2 srcStageMask,
                               vk::PipelineStageFlags2 dstStageMask,
                               const vk::ImageAspectFlags& aspectFlags,
                               uint32_t layerCount = 1) const;

    vk::raii::DeviceMemory allocateBufferMemory(const vk::raii::Buffer& buffer,
                                                vk::MemoryPropertyFlags properties) const;
    vk::raii::DeviceMemory allocateImageMemory(const vk::raii::Image& image, vk::MemoryPropertyFlags properties) const;

    const vk::raii::Device& device() const;
    const vk::raii::PhysicalDevice& physicalDevice() const;
    const vk::raii::Queue& graphicsQueue() const;
    const vk::raii::Queue& presentQueue() const;
    uint32_t graphicsQueueFamilyIndex() const;
    const vk::raii::CommandPool& commandPool() const;

  private:
    void pickPhysicalDevice(const vk::raii::Instance& instance);
    void createLogicalDevice(const vk::raii::SurfaceKHR& surface);
    void createCommandPool();

    bool isDeviceSuitable(vk::raii::PhysicalDevice device) const;
    vk::raii::PhysicalDevice selectBestDevice(const std::vector<vk::raii::PhysicalDevice>& devices) const;

  private:
    vk::raii::Device device_{nullptr};
    vk::raii::PhysicalDevice physicalDevice_{nullptr};
    vk::raii::Queue graphicsQueue_{nullptr};
    vk::raii::Queue presentQueue_{nullptr};
    uint32_t graphicsQueueFamilyIndex_;
    vk::raii::CommandPool commandPool_{nullptr};
};
} // namespace renderer
