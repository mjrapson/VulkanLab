/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

namespace scene
{
struct DirectionalLight
{
    glm::vec3 direction{0.0f};
    glm::vec3 colour{1.0f};
};
} // namespace scene
