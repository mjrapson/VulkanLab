// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <string>

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

    void createVulkanInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createGraphicsPipeline();
    void createCommandPool();
    void createCommandBuffer();
    void createSyncObjects();

    std::vector<char const*> getRequiredExtensions() const;
    std::vector<char const*> getRequiredLayers() const;
    bool isDeviceSuitable(vk::raii::PhysicalDevice device) const;
    vk::raii::PhysicalDevice
    selecteBestDevice(const std::vector<vk::raii::PhysicalDevice>& devices) const;

  private:
    bool glfwInitialised_{false};
    GLFWwindow* window_{nullptr};

    vk::raii::Context context_;
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debugMessenger_{nullptr};
    vk::raii::SurfaceKHR surface_{nullptr};
    vk::raii::PhysicalDevice physicalDevice_{nullptr};
    vk::raii::Device device_{nullptr};
    vk::raii::Queue graphicsQueue_{nullptr};
    vk::raii::Queue presentQueue_{nullptr};
    vk::raii::SwapchainKHR swapchain_{nullptr};
    vk::raii::PipelineLayout pipelineLayout_{nullptr};
    vk::raii::Pipeline graphicsPipeline_{nullptr};
    vk::raii::CommandPool commandPool_{nullptr};
    vk::raii::CommandBuffer commandBuffer_{nullptr};
    vk::raii::Semaphore presentCompleteSemaphore_{nullptr};
    vk::raii::Semaphore renderFinishedSemaphore_{nullptr};
    vk::raii::Fence drawFence_{nullptr};
    uint32_t graphicsQueueFamilyIndex_;
    vk::Extent2D swapchainExtent_;
    vk::SurfaceFormatKHR surfaceFormat_;
    std::vector<vk::Image> swapchainImages_;
    std::vector<vk::raii::ImageView> swapchainImageViews_;
};