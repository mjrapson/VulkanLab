/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "scene/directional_light.h"

#include <assets/prefab.h>
#include <core/handle.h>
#include <renderer/camera.h>

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace scene
{
struct Entity
{
    glm::mat4 transform;
    std::optional<core::Handle<assets::Prefab>> prefabHandle;
    std::optional<renderer::Camera> camera;
    std::optional<DirectionalLight> directionalLight;
    std::vector<Entity> children;
};
} // namespace scene
