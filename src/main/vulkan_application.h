// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <renderer/camera.h>

#include <memory>

namespace renderer
{
class Renderer;
} // namespace renderer

namespace window
{
class Window;
} // namespace window

namespace world
{
class World;
} // namespace world

class VulkanApplication
{
    enum class ApplicationState
    {
        SceneLoading,
        SceneActive,
    };

  public:
    VulkanApplication(window::Window& window, renderer::Renderer& renderer);
    ~VulkanApplication();

    void run();

  private:
    void updateCamera(float deltaTime);

  private:
    window::Window& window_;
    renderer::Renderer& renderer_;
    renderer::Camera camera_;
    ApplicationState currentState_{ApplicationState::SceneLoading};
    std::unique_ptr<world::World> activeWorld_{nullptr};
    std::unique_ptr<world::World> pendingWorld_{nullptr};
};
