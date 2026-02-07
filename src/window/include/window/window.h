/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/input_handler.h>

#include <vulkan/vulkan_raii.hpp>

#include <string_view>

struct GLFWwindow;

namespace window
{
class Surface;

class Window
{
  public:
    Window(int width, int height, std::string_view title);
    ~Window();

    void windowResized(int width, int height);
    void keyPressed(int key, int scancode, int action, int mods);

    std::span<const char* const> requiredExtensions() const;
    vk::raii::SurfaceKHR createVulkanSurface(const vk::raii::Instance& instance) const;

    void pollEvents();
    bool shouldClose() const;

    bool resized() const;
    int width() const;
    int height() const;

    const core::InputHandler& inputHandler() const;

  private:
    core::InputHandler inputHandler_;
    int width_{0};
    int height_{0};
    bool resized_{false};

    GLFWwindow* window_{nullptr};
};
} // namespace window
