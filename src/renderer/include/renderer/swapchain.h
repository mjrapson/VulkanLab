/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class Swapchain
{
  public:
    Swapchain(const vk::raii::Device& device,
              const vk::raii::PhysicalDevice& physicalDevice,
              const vk::raii::SurfaceKHR& surface);

    void initialize(uint32_t preferredFramesInFlight, vk::Extent2D windowExtent);
    bool acquireNextImage();
    void present(const vk::raii::Queue& presentQueue);

    void markOutOfDate();
    bool outOfDate() const;

    vk::Image currentImage() const;
    const vk::raii::ImageView& currentImageView() const;
    const vk::raii::Semaphore& currentPresentCompleteSemaphore() const;
    const vk::raii::Semaphore& currentRenderFinishedSemaphore() const;

    vk::Extent2D extent() const;
    vk::Format imageFormat() const;

  private:
    const vk::raii::Device& device_;
    const vk::raii::PhysicalDevice& physicalDevice_;
    const vk::raii::SurfaceKHR& surface_;

    bool initialized_{false};
    bool outOfDate_{true};

    vk::raii::SwapchainKHR swapchain_{nullptr};
    vk::Extent2D extent_;
    vk::Format imageFormat_;
    std::vector<vk::Image> images_;
    std::vector<vk::raii::ImageView> imageViews_;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;

    uint32_t currentFrameIndex_{0};
    uint32_t maxFramesInFlight_{0};
    uint32_t currentImageIndex_{0};
};
} // namespace renderer
