/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

#include <span>

namespace core
{
struct Box
{
    glm::vec3 min;
    glm::vec3 max;
};

inline Box enclosePoints(std::span<const glm::vec3> points)
{
    assert(!points.empty());

    auto min = points[0];
    auto max = points[0];

    for (const auto& point : points)
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    return Box{min, max};
}
} // namespace core
