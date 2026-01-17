// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <string>

namespace assets
{
class AssetDatabase;
}

namespace renderer
{
class GpuDevice;
class Renderer;
} // namespace renderer

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

    std::unique_ptr<renderer::GpuDevice> gpuDevice_{nullptr};
    std::unique_ptr<renderer::Renderer> renderer_{nullptr};
};
