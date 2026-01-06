// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

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
