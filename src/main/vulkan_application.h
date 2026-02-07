// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <renderer/camera.h>

namespace renderer
{
class Renderer;
} // namespace renderer

namespace window
{
class Window;
} // namespace window

class VulkanApplication
{
  public:
    VulkanApplication(window::Window& window, renderer::Renderer& renderer);

    void run();

  private:
    void updateCamera(float deltaTime);

  private:
    window::Window& window_;
    renderer::Renderer& renderer_;
    renderer::Camera camera_;
};
