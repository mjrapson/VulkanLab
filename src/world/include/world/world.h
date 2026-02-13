/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "entity.h"
#include "world/components/directional_light_component.h"
#include "world/components/render_component.h"
#include "world/components/transform_component.h"
#include "world/systems/render_system.h"

#include <renderer/handles.h>

#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace renderer
{
class Camera;
class Renderer;
} // namespace renderer

namespace world
{
struct Prefab;

class World
{
  public:
    World(renderer::Renderer& renderer);

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World(World&& other) = delete;
    World& operator=(World&& other) = delete;

    Entity createEntity();
    void destroyEntity(Entity entity);

    void addPrefab(const std::string& name, std::unique_ptr<Prefab> prefab);
    Prefab* prefab(const std::string& name) const;

    void setSkybox(renderer::SkyboxHandle handle);
    const std::optional<renderer::SkyboxHandle>& activeSkybox() const;

    const DirectionalLightComponent& directionalLight() const;

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
    // Components
    std::unordered_map<Entity, RenderComponent> renderComponents_;
    std::unordered_map<Entity, TransformComponent> transformComponents_;
    DirectionalLightComponent directionalLight_;

    // Systems
    RenderSystem renderSystem_;

    // State
    std::optional<renderer::SkyboxHandle> activeSkybox_;

    std::unordered_map<std::string, std::unique_ptr<Prefab>> prefabs_;
    Entity nextEntity{0};
};
} // namespace world
