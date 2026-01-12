/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "entity.h"
#include "world/components/camera_component.h"
#include "world/components/render_component.h"
#include "world/components/transform_component.h"

#include <stdexcept>
#include <unordered_map>

namespace world
{
class World
{
  public:
    Entity createEntity()
    {
        return nextEntity++;
    }

    void destroyEntity(Entity entity)
    {
        cameraComponents_.erase(entity);
        renderComponents_.erase(entity);
        transformComponents_.erase(entity);
    }

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
        static_assert(std::is_same_v<Component, CameraComponent> || std::is_same_v<Component, RenderComponent>
                          || std::is_same_v<Component, TransformComponent>,
                      "Component type unknown");

        if constexpr (std::is_same_v<Component, CameraComponent>)
        {
            return cameraComponents_;
        }
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
    std::unordered_map<Entity, CameraComponent> cameraComponents_;
    std::unordered_map<Entity, RenderComponent> renderComponents_;
    std::unordered_map<Entity, TransformComponent> transformComponents_;

  private:
    Entity nextEntity{0};
};
} // namespace world
