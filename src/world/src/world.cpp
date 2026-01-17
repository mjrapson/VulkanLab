/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/world.h"

#include <assets/asset_database.h>
#include <scene/scene.h>

namespace world
{
World::World(renderer::Renderer& renderer)
    : renderSystem_{renderer, *this}
{
}

World::World(const scene::Scene& scene, const assets::AssetDatabase& assetDatabase, renderer::Renderer& renderer)
    : World(renderer)
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
    }
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

void World::update(const renderer::Camera& camera)
{
    renderSystem_.update(camera);
}
} // namespace world
