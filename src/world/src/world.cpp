/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/world.h"

#include <assets/prefab.h>
#include <assets/skybox.h>

namespace world
{
World::World(renderer::Renderer& renderer)
    : renderSystem_{renderer, *this}
{
}

World::~World() = default;

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

void World::addPrefab(const std::string& name, std::unique_ptr<assets::Prefab> prefab)
{
    assert(!prefabs_.contains(name) && "Prefab already exists and will be overwritten");
    prefabs_[name] = std::move(prefab);
}

assets::Prefab* World::prefab(std::string_view name) const
{
    if (auto itr = prefabs_.find(std::string{name}); itr != prefabs_.end())
    {
        return itr->second.get();
    }

    return nullptr;
}

void World::addSkybox(const std::string& name, std::unique_ptr<assets::Skybox> skybox)
{
    assert(!skyboxes_.contains(name) && "Skybox already exists and will be overwritten");
    skyboxes_[name] = std::move(skybox);
}

void World::setActiveSkybox(std::string_view name)
{
    if (auto itr = skyboxes_.find(std::string{name}); itr != skyboxes_.end())
    {
        environment_.skybox = itr->second.get();
    }
    else
    {
        assert(false && "Skybox not found - setting active skybox to nullptr");
        environment_.skybox = nullptr;
    }
}

void World::setGlobalLightDirection(const glm::vec3& direction)
{
    environment_.directionalLight.direction = direction;
}

const World::Environment& World::environment() const
{
    return environment_;
}

void World::initialize()
{
    renderSystem_.initialize();
}

void World::update(const renderer::Camera& camera)
{
    renderSystem_.update(camera);
}
} // namespace world
