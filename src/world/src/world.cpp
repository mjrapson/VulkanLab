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
    std::apply(
        [entity](auto&... storage)
        {
            (storage.erase(entity), ...);
        },
        components_);
}

void World::addPrefab(const std::string& name, std::unique_ptr<Prefab> prefab)
{
    assert(!prefabs_.contains(name) && "Prefab already exists and will be overwritten");
    prefabs_[name] = std::move(prefab);
}

Prefab* World::prefab(std::string_view name) const
{
    if (auto itr = prefabs_.find(std::string{name}); itr != prefabs_.end())
    {
        return itr->second.get();
    }

    return nullptr;
}

void World::setSkybox(renderer::SkyboxHandle handle)
{
    activeSkybox_ = handle;
}

const std::optional<renderer::SkyboxHandle>& World::activeSkybox() const
{
    return activeSkybox_;
}

void World::setDirectionalLight(const glm::vec3& direction)
{
    directionalLight_.direction = direction;
}

void World::update(const renderer::Camera& camera)
{
    renderSystem_.update(camera);
}
} // namespace world
