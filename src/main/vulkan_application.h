// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <string>

class GpuDevice;

struct GLFWwindow;

class VulkanApplication
{
  public:
    VulkanApplication();
    ~VulkanApplication();

    void init(int windowWidth, int windowHeight, const std::string& windowTitle);
    void run();

  private:
    void initGlfw();
    void initWindow(int windowWidth, int windowHeight, const std::string& windowTitle);
    void initVulkan();

    void createInstance();
    void createDebugMessenger();
    void createSurface();
    void createSwapchain();
    void createImageViews();
    void createGraphicsPipeline();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    void recreateSwapChain();

    void drawFrame();
    void recordCommands(uint32_t imageIndex, const vk::raii::CommandBuffer& commandBuffer);

  private:
    bool glfwInitialised_{false};
    GLFWwindow* window_{nullptr};

    vk::raii::Context context_;
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debugMessenger_{nullptr};
    vk::raii::SurfaceKHR surface_{nullptr};

    std::unique_ptr<GpuDevice> gpuDevice_{nullptr};

    vk::raii::SwapchainKHR swapchain_{nullptr};
    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline graphicsPipeline_{nullptr};
    vk::raii::CommandPool commandPool_{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence> drawFences_;
    vk::Extent2D swapchainExtent_;
    vk::SurfaceFormatKHR surfaceFormat_;
    std::vector<vk::Image> swapchainImages_;
    std::vector<vk::raii::ImageView> swapchainImageViews_;
    uint32_t currentFrameIndex_{0};
};