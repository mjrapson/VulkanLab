// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include "gltf_loader.h"
#include "image_loader.h"

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

VulkanApplication::LoadResult loadSceneData(const std::filesystem::path& path, renderer::Renderer& renderer)
{
    auto world = std::make_unique<world::World>(renderer);

    // 1. Load scene
    auto scene = scene::loadScene(path);

    // 2. Load assets
    for (const auto& prefabDef : scene->prefabs)
    {
        world->addPrefab(prefabDef.name, loadGLTFModel(core::getPrefabsDir() / prefabDef.path));
    }

    for (const auto& skyboxDef : scene->skyboxes)
    {
        auto skyboxImages = std::array<std::unique_ptr<assets::ImageData>, 6>{};
        skyboxImages[0] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.pxPath);
        skyboxImages[1] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.pyPath);
        skyboxImages[2] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.pzPath);
        skyboxImages[3] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.nxPath);
        skyboxImages[4] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.nyPath);
        skyboxImages[5] = createImageFromPath(core::getSkyboxesDir() / skyboxDef.nzPath);

        world->addSkybox(skyboxDef.name, std::make_unique<assets::Skybox>(std::move(skyboxImages)));
    }

    // 3. Create entities
    for (const auto& sceneEntity : scene->entities)
    {
        auto entity = world->createEntity();

        if (sceneEntity.renderComponent.has_value())
        {
            auto& renderComponent = world->addComponent<world::RenderComponent>(entity);
            renderComponent.prefab = world->prefab(sceneEntity.renderComponent->prefabId);
        }
        if (sceneEntity.transformComponent.has_value())
        {
            auto& transformComponent = world->addComponent<world::TransformComponent>(entity);
            transformComponent.position = sceneEntity.transformComponent->position;
            transformComponent.rotation = sceneEntity.transformComponent->rotation;
            transformComponent.scale = sceneEntity.transformComponent->scale;
        }
    }

    world->setGlobalLightDirection(scene->directionalLight.direction);

    return VulkanApplication::LoadResult{std::move(world)};
}

VulkanApplication::VulkanApplication(window::Window& window, renderer::Renderer& renderer)
    : window_{window},
      renderer_{renderer}
{
    loadingScreen_ = std::make_unique<assets::LoadingScreen>(
        createImageFromPath(core::getTexturesDir() / "loading_screen.png"));
    loadingScreen_->setRenderHandle(
        renderer_.addLoadingScreenImage(loadingScreen_->width(), loadingScreen_->height(), loadingScreen_->data()));
}

VulkanApplication::~VulkanApplication() = default;

void VulkanApplication::run()
{
    spdlog::info("Running");

    currentState_ = ApplicationState::SceneLoading;
    loadScene(core::getScenesDir() / "demo.json");

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
                /* Reset camera - this should really be defined by the scene where it wants to start */
                camera_.position = glm::vec3{0.0f, 8.0f, 24.0f};
                currentState_ = ApplicationState::SceneActive;

                /* Update the active world */
                auto result = sceneLoadFuture_.get();
                activeWorld_ = std::move(result.world);
                activeWorld_->initialize();
            }
            else
            {
                renderer_.renderLoadingScreen(*loadingScreen_->renderHandle());
            }
        }
        else if (currentState_ == ApplicationState::SceneActive && activeWorld_)
        {
            activeWorld_->update(camera_);
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
                                  [this, scenePath]()
                                  {
                                      return loadSceneData(scenePath, renderer_);
                                  });
}
