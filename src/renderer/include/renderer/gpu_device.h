// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <filesystem>
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

    vk::raii::CommandPool createCommandPool() const;

    vk::raii::CommandBuffers createCommandBuffers(const vk::raii::CommandPool& pool, uint32_t count) const;
    void submitCommandBuffer(const vk::CommandBuffer& cmd) const;
    void submitCommandBuffer(const vk::CommandBuffer& cmd,
                             std::span<vk::Semaphore> waitSemaphores,
                             const vk::PipelineStageFlags& waitStageMask,
                             std::span<vk::Semaphore> signalSemaphores,
                             const vk::Fence& fence) const;

    vk::raii::DescriptorPool createDescriptorPool(uint32_t maxSets, std::span<vk::DescriptorPoolSize> poolSizes) const;

    vk::raii::DescriptorSetLayout
    createDescriptorSetLayout(const std::span<const vk::DescriptorSetLayoutBinding>& bindings) const;

    vk::raii::DescriptorSets createDescriptorSets(const vk::raii::DescriptorSetLayout& layout,
                                                  const vk::raii::DescriptorPool& pool,
                                                  uint32_t count) const;

    vk::raii::ShaderModule createShaderModule(const std::filesystem::path& filePath) const;

    vk::raii::Buffer createBuffer(const vk::DeviceSize& size,
                                  const vk::BufferUsageFlags& usage,
                                  const vk::SharingMode& sharingMode) const;

    vk::raii::Buffer createVertexBuffer(const vk::DeviceSize& size) const;
    vk::raii::Buffer createIndexBuffer(const vk::DeviceSize& size) const;
    vk::raii::Buffer createStagingBuffer(const vk::DeviceSize& size) const;
    vk::raii::Buffer createUniformBuffer(const vk::DeviceSize& size) const;

    void copyBuffer(const vk::raii::CommandBuffer& cmd,
                    const vk::raii::Buffer& source,
                    const vk::raii::Buffer& destination,
                    const vk::DeviceSize& size,
                    const vk::DeviceSize& destinationOffset = 0) const;

    void copyBufferToImage(const vk::raii::CommandBuffer& cmd,
                           const vk::raii::Buffer& source,
                           const vk::raii::Image& destination,
                           uint32_t width,
                           uint32_t height,
                           uint32_t layers = 1) const;

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
    vk::raii::DeviceMemory allocateDeviceBufferMemory(const vk::raii::Buffer& buffer) const;
    vk::raii::DeviceMemory allocateStagingBufferMemory(const vk::raii::Buffer& buffer) const;
    vk::raii::DeviceMemory allocateImageMemory(const vk::raii::Image& image, vk::MemoryPropertyFlags properties) const;

    vk::DeviceSize calculateAlignedUboStride(size_t uboSize) const;

    bool exceedsPushConstantLimit(size_t size) const;

    const vk::raii::Device& device() const;
    const vk::raii::PhysicalDevice& physicalDevice() const;
    const vk::raii::Queue& presentQueue() const;

  private:
    void pickPhysicalDevice(const vk::raii::Instance& instance);
    void createLogicalDevice(const vk::raii::SurfaceKHR& surface);

    bool isDeviceSuitable(vk::raii::PhysicalDevice device) const;
    vk::raii::PhysicalDevice selectBestDevice(const std::vector<vk::raii::PhysicalDevice>& devices) const;

  private:
    vk::raii::Device device_{nullptr};
    vk::raii::PhysicalDevice physicalDevice_{nullptr};
    vk::raii::Queue graphicsQueue_{nullptr};
    vk::raii::Queue presentQueue_{nullptr};
    uint32_t graphicsQueueFamilyIndex_;
};
} // namespace renderer
