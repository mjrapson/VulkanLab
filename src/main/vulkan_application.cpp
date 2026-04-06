// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include <core/file_system.h>
#include <core/input_handler.h>
#include <renderer/renderer.h>
#include <scene/scene.h>
#include <scene/scene_loader.h>
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
}

VulkanApplication::~VulkanApplication() = default;

void VulkanApplication::run()
{
    spdlog::info("Loading scene");
    auto scene = scene::loadSceneFromFolder(core::getScenesDir() / "demo");

    spdlog::info("Initializing assets");
    renderSystem_.initialize(*scene);

    // Snap to the first camera we find for now, assuming we'll just export one
    // or default to a simple camera slightly above and behind the origin of the scene
    const auto cameras = collectCameras(*scene);
    if (cameras.empty())
    {
        camera_.position = glm::vec3{0.0f, 1.0f, -3.0f};
    }
    else
    {
        camera_ = cameras.at(0);
        camera_.aspectRatio = getAspectRatio(window_.width(), window_.height());
    }

    spdlog::info("Running...");
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

        renderSystem_.update(*scene, camera_);

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
