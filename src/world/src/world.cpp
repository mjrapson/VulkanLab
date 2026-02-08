/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/world.h"

#include <assets/asset_database.h>
#include <renderer/gpu_resource_cache.h>
#include <scene/scene.h>

namespace world
{
World::World(const scene::Scene& scene, const assets::AssetDatabase& assetDatabase, renderer::Renderer& renderer)
    : renderSystem_{renderer, *this}
{
    for (const auto& sceneEntity : scene.entities)
    {
        auto entity = createEntity();

        if (sceneEntity.renderComponent.has_value())
        {
            auto& renderComponent = addComponent<RenderComponent>(entity);
            renderComponent.prefab = assetDatabase.prefabs().at(sceneEntity.renderComponent->prefabId).get();
        }
        if (sceneEntity.transformComponent.has_value())
        {
            auto& transformComponent = addComponent<TransformComponent>(entity);
            transformComponent.position = sceneEntity.transformComponent->position;
            transformComponent.rotation = sceneEntity.transformComponent->rotation;
            transformComponent.scale = sceneEntity.transformComponent->scale;
        }

        activeSkybox_ = assetDatabase.skyboxes().at(scene.camera.skybox)->handle();
        directionalLight_.direction = scene.directionalLight.direction;
    }

    gpuResourcesFuture_ = renderSystem_.initialize(assetDatabase);
}

Entity World::createEntity()
{
    return nextEntity++;
}

void World::destroyEntity(Entity entity)
{
    renderComponents_.erase(entity);
    transformComponents_.erase(entity);
}

const std::optional<assets::SkyboxHandle>& World::activeSkybox() const
{
    return activeSkybox_;
}

const DirectionalLightComponent& World::directionalLight() const
{
    return directionalLight_;
}

std::unique_ptr<renderer::GpuResourceCache> World::gpuResources()
{
    return gpuResourcesFuture_.get();
}

bool World::isReady() const
{
    return gpuResourcesFuture_.valid()
           && gpuResourcesFuture_.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
}

void World::update(const renderer::Camera& camera)
{
    renderSystem_.update(camera);
}
} // namespace world
