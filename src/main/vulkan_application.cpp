// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include <assets/asset_database.h>
#include <assets/gltf_loader.h>
#include <assets/image_loader.h>
#include <core/file_system.h>
#include <core/input_handler.h>
#include <renderer/camera.h>
#include <renderer/gpu_device.h>
#include <renderer/renderer.h>
#include <scene/scene.h>
#include <scene/scene_loader.h>
#include <window/window.h>
#include <world/systems/render_system.h>
#include <world/world.h>

#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <chrono>

float getAspectRatio(int width, int height)
{
    return static_cast<float>(width) / static_cast<float>(height);
}

VulkanApplication::VulkanApplication(window::Window& window, renderer::Renderer& renderer)
    : window_{window},
      renderer_{renderer}
{
}

void VulkanApplication::run()
{
    spdlog::info("Running");

    auto scene = scene::loadScene(core::getScenesDir() / "demo.json");
    camera_.setPosition(glm::vec3{0.0f, 8.0f, 24.0f});

    // Probably show some loading screen here...
    // Move to separate func
    auto db = assets::AssetDatabase{};
    for (auto& prefabDef : scene->prefabs)
    {
        db.addPrefab(prefabDef.name, assets::loadGLTFModel(core::getPrefabsDir() / prefabDef.path));
    }

    for (auto& skyboxDef : scene->skyboxes)
    {
        auto skybox = std::make_unique<assets::Skybox>();
        skybox->setImage(0, assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.pxPath));
        skybox->setImage(1, assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.pyPath));
        skybox->setImage(2, assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.pzPath));
        skybox->setImage(3, assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.nxPath));
        skybox->setImage(4, assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.nyPath));
        skybox->setImage(5, assets::createImageFromPath(core::getSkyboxesDir() / skyboxDef.nzPath));

        db.addSkybox(skyboxDef.name, std::move(skybox));
    }

    renderer_.setResources(db);

    auto world = world::World{*scene, db, renderer_};
    // ...end loading screen

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
            camera_.setAspectRatio(getAspectRatio(window_.width(), window_.height()));
        }

        updateCamera(static_cast<float>(deltaTime));

        world.update(camera_);

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

    auto worldUp = glm::vec3(0, 1, 0);
    auto forward = glm::normalize(camera_.front());
    auto right = glm::normalize(glm::cross(forward, worldUp));
    auto up = glm::normalize(glm::cross(right, forward));

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
        camera_.setYaw(camera_.yaw() - (turnSpeed * deltaTime));
    }

    if (inputHandler.isKeyPressed(GLFW_KEY_D))
    {
        camera_.setYaw(camera_.yaw() + (turnSpeed * deltaTime));
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
        camera_.setPosition(camera_.position() + movement);
    }
}
