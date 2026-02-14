/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "entity.h"
#include "world/components/render_component.h"
#include "world/components/transform_component.h"
#include "world/directional_light.h"
#include "world/systems/render_system.h"

#include <renderer/handles.h>

#include <cassert>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
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
    template <typename Component>
    using ComponentStorage = std::unordered_map<Entity, Component>;

    using RenderComponentStorage = ComponentStorage<RenderComponent>;
    using TransformComponentStorage = ComponentStorage<TransformComponent>;

  public:
    World(renderer::Renderer& renderer);

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World(World&& other) = delete;
    World& operator=(World&& other) = delete;

    Entity createEntity();
    void destroyEntity(Entity entity);

    void addPrefab(const std::string& name, std::unique_ptr<Prefab> prefab);
    Prefab* prefab(std::string_view name) const;

    void setSkybox(renderer::SkyboxHandle handle);
    const std::optional<renderer::SkyboxHandle>& activeSkybox() const;

    void setDirectionalLight(const glm::vec3& direction);

    void update(const renderer::Camera& camera);

    template <typename Component, typename... Args>
    Component& addComponent(Entity entity, Args&&... args)
    {
        auto& storage = getStorage<Component>();
        auto [itr, inserted] = storage.emplace(entity, Component(std::forward<Args>(args)...));

        assert(inserted && "Component already exists on this entity");

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
    const auto& getAllComponents()
    {
        return getStorage<Component>();
    }

  private:
    template <typename Component>
    auto& getStorage()
    {
        return std::get<ComponentStorage<Component>>(components_);
    }

    template <typename Component>
    const auto& getStorage() const
    {
        return std::get<ComponentStorage<Component>>(components_);
    }

  private:
    // Components
    std::tuple<RenderComponentStorage, TransformComponentStorage> components_;

    // Systems
    RenderSystem renderSystem_;

    // State
    std::optional<renderer::SkyboxHandle> activeSkybox_;
    DirectionalLight directionalLight_;

    std::unordered_map<std::string, std::unique_ptr<Prefab>> prefabs_;
    Entity nextEntity{0};
};
} // namespace world
