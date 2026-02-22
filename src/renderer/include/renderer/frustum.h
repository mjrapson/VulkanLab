/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/box.h>

#include <glm/glm.hpp>

namespace renderer
{

struct Frustum
{
    enum class Corner
    {
        NearTopLeft = 0,
        NearTopRight,
        NearBottomLeft,
        NearBottomRight,
        FarTopLeft,
        FarTopRight,
        FarBottomLeft,
        FarBottomRight
    };

    std::array<glm::vec3, 8> corners;

    glm::vec3& operator[](Corner corner)
    {
        return corners[static_cast<size_t>(corner)];
    }

    const glm::vec3& operator[](Corner corner) const
    {
        return corners[static_cast<size_t>(corner)];
    }

    glm::vec3 midPoint() const
    {
        auto pointSum = glm::vec3{0.0f};
        for (const auto& point : corners)
        {
            pointSum += point;
        }

        return pointSum / 8.0f;
    }

    core::Box boudingBox() const
    {
        return core::enclosePoints(corners);
    }
};

inline Frustum viewTransform(const Frustum& frustum, const glm::mat4& viewMatrix)
{
    auto result = Frustum{};
    for (auto i = size_t{0}; i < frustum.corners.size(); ++i)
    {
        result.corners[i] = glm::vec3{viewMatrix * glm::vec4{frustum.corners[i], 1.0f}};
    }

    return result;
}
} // namespace renderer
