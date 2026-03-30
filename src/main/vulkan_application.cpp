// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include <assets/loaders/image_loader.h>
#include <assets/loading_screen.h>
#include <assets/prefab.h>
#include <assets/skybox.h>
#include <core/file_system.h>
#include <core/input_handler.h>
#include <renderer/camera.h>
#include <renderer/gpu_device.h>
#include <renderer/renderer.h>
#include <scene/scene.h>
#include <scene/scene_loader.h>
#include <scene/systems/render_system.h>
#include <window/window.h>

#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <chrono>

float getAspectRatio(int width, int height)
{
    return static_cast<float>(width) / static_cast<float>(height);
}

std::vector<renderer::Camera> collectCameras(scene::Scene& scene)
{
    auto cameras = std::vector<renderer::Camera>{};
    scene.entityGraph.visit(
        [&cameras](scene::Entity& entity)
        {
            if (entity.camera)
            {
                cameras.push_back(*entity.camera);
            }
        });

    return cameras;
}

VulkanApplication::VulkanApplication(window::Window& window, renderer::Renderer& renderer)
    : window_{window},
      renderer_{renderer},
      renderSystem_{renderer}
{
    camera_.aspectRatio = getAspectRatio(window_.width(), window_.height());

    loadingScreen_ = std::make_unique<assets::LoadingScreen>(
        assets::createImageFromPath(core::getTexturesDir() / "loading_screen.png"));
    loadingScreen_->setRenderHandle(
        renderer_.addLoadingScreenImage(loadingScreen_->width(), loadingScreen_->height(), loadingScreen_->data()));
}

VulkanApplication::~VulkanApplication() = default;

void VulkanApplication::run()
{
    spdlog::info("Running");

    currentState_ = ApplicationState::SceneLoading;
    loadScene(core::getScenesDir() / "demo");

    constexpr auto maxFps = std::chrono::duration<double>(1.0 / 60.0);
    auto lastTime = std::chrono::steady_clock::now();

    while (!window_.shouldClose())
    {
        const auto frameStartTime = std::chrono::steady_clock::now();
        const auto deltaTime = std::chrono::duration<double>(frameStartTime - lastTime).count();
        lastTime = frameStartTime;

        window_.pollEvents();

        if (window_.resized())
        {
            renderer_.windowResized(window_.width(), window_.height());
            camera_.aspectRatio = getAspectRatio(window_.width(), window_.height());
        }

        updateCamera(static_cast<float>(deltaTime));

        /*
            If we are loading a scene and it is ready - transition to it. All CPU data is ready,
            but we need to upload GPU data in this loop as currently we only sync with the GPU on this
            thread.

            If the the async task loading the CPU data for the scene isn't ready, continue displaying
            the loading screen.
        */
        if (currentState_ == ApplicationState::SceneLoading)
        {
            if (sceneLoadFuture_.valid()
                && sceneLoadFuture_.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
            {
                currentState_ = ApplicationState::SceneActive;

                activeScene_ = sceneLoadFuture_.get();

                renderSystem_.initialize(*activeScene_);

                // Snap to the first camera we find for now, assuming we'll just export one
                // or default to a simple camera slightly above and behind the origin of the scene
                const auto cameras = collectCameras(*activeScene_);
                if (cameras.empty())
                {
                    camera_.position = glm::vec3{0.0f, 1.0f, -3.0f};
                }
                else
                {
                    camera_ = cameras.at(0);
                    camera_.aspectRatio = getAspectRatio(window_.width(), window_.height());
                }
            }
            else
            {
                renderer_.renderLoadingScreen(*loadingScreen_->renderHandle());
            }
        }
        else if (currentState_ == ApplicationState::SceneActive && activeScene_)
        {
            renderSystem_.update(*activeScene_, camera_);
        }

        /* Crude FPS limit - this needs to be replaced by a proper frame sync */
        const auto frameFinishTime = std::chrono::steady_clock::now();
        const auto frameDuration = frameFinishTime - frameStartTime;
        if (frameDuration < maxFps)
        {
            std::this_thread::sleep_for(maxFps - frameDuration);
        }
    }
}

void VulkanApplication::updateCamera(float deltaTime)
{
    const auto speed = 15.0f;
    const auto turnSpeed = 45.0f;

    auto movement = glm::vec3{0.0f};

    const auto forward = camera_.front();
    const auto up = camera_.up();
    const auto turningAngle = glm::radians(turnSpeed * deltaTime);

    auto inputHandler = window_.inputHandler();

    if (inputHandler.isKeyPressed(GLFW_KEY_W))
    {
        movement = movement + forward;
    }

    if (inputHandler.isKeyPressed(GLFW_KEY_S))
    {
        movement = movement - forward;
    }

    if (inputHandler.isKeyPressed(GLFW_KEY_A))
    {
        camera_.orientation = glm::normalize(glm::angleAxis(turningAngle, glm::vec3{0, 1, 0}) * camera_.orientation);
    }

    if (inputHandler.isKeyPressed(GLFW_KEY_D))
    {
        camera_.orientation = glm::normalize(glm::angleAxis(-turningAngle, glm::vec3{0, 1, 0}) * camera_.orientation);
    }

    if (inputHandler.isKeyPressed(GLFW_KEY_E))
    {
        movement = movement + up;
    }

    if (inputHandler.isKeyPressed(GLFW_KEY_Q))
    {
        movement = movement - up;
    }

    if (glm::length(movement) > 0.0f)
    {
        movement = glm::normalize(movement) * speed * deltaTime;
        camera_.position = (camera_.position + movement);
    }
}

void VulkanApplication::loadScene(const std::filesystem::path& scenePath)
{
    sceneLoadFuture_ = std::async(std::launch::async,
                                  [scenePath]()
                                  {
                                      return scene::loadSceneFromFolder(scenePath);
                                  });
}
