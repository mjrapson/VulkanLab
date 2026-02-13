// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include "gltf_loader.h"
#include "image_loader.h"

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

void prepareScene()
{
}

VulkanApplication::VulkanApplication(window::Window& window, renderer::Renderer& renderer)
    : window_{window},
      renderer_{renderer}
{
    // auto loadingScreenImage = createImageFromPath(core::getTexturesDir() / "loading_screen.png");
    // if (loadingScreenImage)
    // {
    //     renderer_.setLoadingScreenImage(*loadingScreenImage);
    // }
}

VulkanApplication::~VulkanApplication() = default;

void VulkanApplication::run()
{
    spdlog::info("Running");

    auto scene = scene::loadScene(core::getScenesDir() / "demo.json");
    camera_.setPosition(glm::vec3{0.0f, 8.0f, 24.0f});

    activeWorld_ = std::make_unique<world::World>(renderer_);
    auto pendingAssetData = renderer::AssetData{};

    // Probably show some loading screen here...
    // Move to separate func
    for (auto& prefabDef : scene->prefabs)
    {
        activeWorld_->addPrefab(prefabDef.name,
                                loadGLTFModel(core::getPrefabsDir() / prefabDef.path, pendingAssetData));
    }

    for (auto& skyboxDef : scene->skyboxes)
    {
        auto skyboxData = renderer::SkyboxData{};
        skyboxData.imageData[0] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.pxPath);
        skyboxData.imageData[1] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.pyPath);
        skyboxData.imageData[2] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.pzPath);
        skyboxData.imageData[3] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.nxPath);
        skyboxData.imageData[4] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.nyPath);
        skyboxData.imageData[5] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.nzPath);

        const auto handle = renderer::SkyboxHandleGenerator::generate();

        activeWorld_->setSkybox(handle);

        pendingAssetData.skyboxData.emplace(handle, std::move(skyboxData));
    }

    for (const auto& sceneEntity : scene->entities)
    {
        auto entity = activeWorld_->createEntity();

        if (sceneEntity.renderComponent.has_value())
        {
            auto& renderComponent = activeWorld_->addComponent<world::RenderComponent>(entity);
            renderComponent.prefab = activeWorld_->prefab(sceneEntity.renderComponent->prefabId);
        }
        if (sceneEntity.transformComponent.has_value())
        {
            auto& transformComponent = activeWorld_->addComponent<world::TransformComponent>(entity);
            transformComponent.position = sceneEntity.transformComponent->position;
            transformComponent.rotation = sceneEntity.transformComponent->rotation;
            transformComponent.scale = sceneEntity.transformComponent->scale;
        }

        // directionalLight_.direction = scene.directionalLight.direction;
    }

    renderer_.setData(pendingAssetData);

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

        // if (currentState_ == ApplicationState::SceneLoading && pendingWorld_)
        //{
        //  if (pendingWorld_->isReady())
        //  {
        //      activeWorld_ = std::move(pendingWorld_);
        //      currentState_ = ApplicationState::SceneActive;
        //  }
        //  else
        //  {
        //  renderer_.renderLoadingScreen();
        // }
        // }
        // else if (currentState_ == ApplicationState::SceneActive && activeWorld_)
        {
            updateCamera(static_cast<float>(deltaTime));

            activeWorld_->update(camera_);
        }

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
