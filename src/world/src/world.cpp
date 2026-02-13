/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/world.h"

namespace world
{
World::World(renderer::Renderer& renderer)
    : renderSystem_{renderer, *this}
{
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

void World::addPrefab(const std::string& name, std::unique_ptr<Prefab> prefab)
{
    prefabs_[name] = std::move(prefab);
}

Prefab* World::prefab(const std::string& name) const
{
    return prefabs_.at(name).get();
}

void World::setSkybox(renderer::SkyboxHandle handle)
{
    activeSkybox_ = handle;
}

const std::optional<renderer::SkyboxHandle>& World::activeSkybox() const
{
    return activeSkybox_;
}

const DirectionalLightComponent& World::directionalLight() const
{
    return directionalLight_;
}

void World::update(const renderer::Camera& camera)
{
    renderSystem_.update(camera);
}
} // namespace world
