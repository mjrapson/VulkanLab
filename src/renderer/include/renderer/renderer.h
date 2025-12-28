// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class GpuDevice;

class Renderer
{
  public:
    Renderer(const vk::raii::Instance& instance,
             const vk::raii::SurfaceKHR& surface,
             const GpuDevice& gpuDevice,
             int windowWidth,
             int windowHeight);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&& other) = delete;
    Renderer& operator=(Renderer&& other) = delete;

    void renderFrame();
    void windowResized(int width, int height);

  private:
    void createSwapchain();
    void createSwapchainImageViews();
    void createGraphicsPipeline();
    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createCommandBuffers();
    void createSyncObjects();

    void recreateSwapchain();
    void recordCommands(uint32_t imageIndex, const vk::raii::CommandBuffer& commandBuffer);

  private:
    const vk::raii::Instance& instance_;
    const vk::raii::SurfaceKHR& surface_;
    const GpuDevice& gpuDevice_;

    int windowWidth_{0};
    int windowHeight_{0};
    bool windowResized_{false};
    bool windowMinimized_{false};

    vk::raii::SwapchainKHR swapchain_{nullptr};
    vk::Extent2D swapchainExtent_;
    vk::SurfaceFormatKHR surfaceFormat_;
    std::vector<vk::Image> swapchainImages_;
    std::vector<vk::raii::ImageView> swapchainImageViews_;
    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline graphicsPipeline_{nullptr};
    vk::raii::CommandPool commandPool_{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence> drawFences_;
    uint32_t currentFrameIndex_{0};

    vk::raii::Buffer vertexBuffer_{nullptr};
    vk::raii::DeviceMemory vertexBufferMemory_{nullptr};
    vk::raii::Buffer indexBuffer_{nullptr};
    vk::raii::DeviceMemory indexBufferMemory_{nullptr};
};
} // namespace renderer