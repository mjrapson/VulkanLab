/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "entity.h"
#include "world/components/render_component.h"
#include "world/components/transform_component.h"
#include "world/systems/render_system.h"

#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace scene
{
struct Scene;
}

namespace assets
{
class AssetDatabase;
struct Skybox;
} // namespace assets

namespace renderer
{
class Camera;
class Renderer;
} // namespace renderer

namespace world
{
class World
{
  public:
    World(renderer::Renderer& renderer);
    World(const scene::Scene& scene, const assets::AssetDatabase& assetDatabase, renderer::Renderer& renderer);

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World(World&& other) = delete;
    World& operator=(World&& other) = delete;

    Entity createEntity();
    void destroyEntity(Entity entity);

    void setActiveSkybox(assets::Skybox* skybox);
    assets::Skybox* activeSkybox() const;

    void update(const renderer::Camera& camera);

    template <typename Component, typename... Args>
    Component& addComponent(Entity entity, Args&&... args)
    {
        auto& storage = getStorage<Component>();
        auto [itr, inserted] = storage.emplace(entity, Component(std::forward<Args>(args)...));
        if (!inserted)
        {
            throw std::logic_error("Component already exists on this entity");
        }

        return itr->second;
    }

    template <typename Component>
    bool hasComponent(Entity entity) const
    {
        const auto& storage = getStorage<Component>();
        return storage.find(entity) != storage.end();
    }

    template <typename Component>
    Component* getComponent(Entity entity)
    {
        auto& storage = getStorage<Component>();
        if (auto itr = storage.find(entity); itr != storage.end())
        {
            return &itr->second;
        }
        return nullptr;
    }

    template <typename Component>
    auto& getAllComponents()
    {
        return getStorage<Component>();
    }

    template <typename Component>
    const auto& getAllComponents() const
    {
        return getStorage<Component>();
    }

  private:
    template <typename Component>
    auto& getStorage()
    {
        static_assert(std::is_same_v<Component, RenderComponent> || std::is_same_v<Component, TransformComponent>,
                      "Component type unknown");

        if constexpr (std::is_same_v<Component, RenderComponent>)
        {
            return renderComponents_;
        }
        if constexpr (std::is_same_v<Component, TransformComponent>)
        {
            return transformComponents_;
        }
    }

  private:
    std::unordered_map<Entity, RenderComponent> renderComponents_;
    std::unordered_map<Entity, TransformComponent> transformComponents_;
    assets::Skybox* activeSkybox_{nullptr};

  private:
    Entity nextEntity{0};
    RenderSystem renderSystem_;
};
} // namespace world
