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
    void pickPhysicalDevice();
    void createLogicalDevice();

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
    vk::raii::PhysicalDevice physicalDevice_{nullptr};
    vk::raii::Device device_{nullptr};
};