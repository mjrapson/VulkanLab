// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <string>

class GpuDevice;
class Renderer;

struct GLFWwindow;

class VulkanApplication
{
  public:
    VulkanApplication();
    ~VulkanApplication();

    void init(int windowWidth, int windowHeight, const std::string& windowTitle);
    void run();

    void windowResized(int width, int height);

  private:
    void initGlfw();
    void initWindow(int windowWidth, int windowHeight, const std::string& windowTitle);
    void initVulkan(int windowWidth, int windowHeight);

    void createInstance();
    void createDebugMessenger();
    void createSurface();

  private:
    bool glfwInitialised_{false};
    GLFWwindow* window_{nullptr};

    vk::raii::Context context_;
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debugMessenger_{nullptr};
    vk::raii::SurfaceKHR surface_{nullptr};

    std::unique_ptr<GpuDevice> gpuDevice_{nullptr};
    std::unique_ptr<Renderer> renderer_{nullptr};
};