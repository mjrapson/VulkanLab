// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <renderer/camera.h>

#include <filesystem>
#include <future>
#include <memory>

namespace renderer
{
class Renderer;
} // namespace renderer

namespace window
{
class Window;
} // namespace window

namespace assets
{
class LoadingScreen;
}

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

    struct LoadResult
    {
        std::unique_ptr<world::World> world;
        glm::vec3 cameraStartPosition;
    };

    void run();

  private:
    void updateCamera(float deltaTime);
    void loadScene(const std::filesystem::path& scenePath);

  private:
    window::Window& window_;
    renderer::Renderer& renderer_;
    renderer::Camera camera_;

    ApplicationState currentState_{ApplicationState::SceneLoading};
    std::unique_ptr<assets::LoadingScreen> loadingScreen_{nullptr};
    std::future<LoadResult> sceneLoadFuture_;

    std::unique_ptr<world::World> activeWorld_{nullptr};
};
