// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <renderer/camera.h>
#include <scene/systems/render_system.h>

#include <filesystem>

namespace renderer
{
class Renderer;
} // namespace renderer

namespace window
{
class Window;
} // namespace window

namespace scene
{
struct Scene;
} // namespace scene

class VulkanApplication
{
  public:
    VulkanApplication(window::Window& window, renderer::Renderer& renderer);
    ~VulkanApplication();

    void run();

  private:
    void updateCamera(float deltaTime);
    void loadScene(const std::filesystem::path& scenePath);

  private:
    window::Window& window_;
    renderer::Renderer& renderer_;
    renderer::Camera camera_;
    scene::RenderSystem renderSystem_;
};
