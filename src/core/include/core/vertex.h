/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

namespace core
{
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 textureUV;
};
} // namespace core
