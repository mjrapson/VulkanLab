/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace scene
{
struct Prefab
{
    std::string name;
    std::string path;
};

struct TransformComponent
{
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

struct RenderComponent
{
    std::string prefabId;
};

struct Entity
{
    std::string name;
    std::optional<TransformComponent> transformComponent;
    std::optional<RenderComponent> renderComponent;
};

struct Scene
{
    std::vector<Prefab> prefabs;
    std::vector<Entity> entities;
};
} // namespace scene
