/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/frustum.h"

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace renderer
{
struct Camera
{
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat orientation{1, 0, 0, 0};
    float fieldOfView{45.0f};
    float nearPlane{0.1f};
    float farPlane{1000.0f};
    float aspectRatio{1.0f};

    glm::vec3 front() const
    {
        return orientation * glm::vec3{0, 0, -1};
    }

    glm::vec3 right() const
    {
        return orientation * glm::vec3{1, 0, 0};
    }

    glm::vec3 up() const
    {
        return orientation * glm::vec3{0, 1, 0};
    }

    glm::mat4 projection() const
    {
        auto projection = glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
        projection[1][1] *= -1.0f;

        return projection;
    }

    glm::mat4 view() const
    {
        return glm::lookAt(position, position + front(), up());
    }

    Frustum frustumSlice(float nearDistance, float farDistance) const
    {
        const auto frontDir = front();
        const auto upDir = up();
        const auto rightDir = right();

        const auto nearPlaneHeight = nearDistance * glm::tan(glm::radians(fieldOfView) / 2) * 2.0f;
        const auto nearPlaneWidth = nearPlaneHeight * aspectRatio;
        const auto nearPlaneMidPoint = position + frontDir * nearDistance;

        const auto farPlaneHeight = farDistance * glm::tan(glm::radians(fieldOfView) / 2) * 2.0f;
        const auto farPlaneWidth = farPlaneHeight * aspectRatio;
        const auto farPlaneMidPoint = position + frontDir * farDistance;

        auto frustum = Frustum{};
        frustum[Frustum::Corner::NearTopLeft] = nearPlaneMidPoint + (upDir * (nearPlaneHeight / 2))
                                                - (rightDir * (nearPlaneWidth / 2));
        frustum[Frustum::Corner::NearTopRight] = nearPlaneMidPoint + (upDir * (nearPlaneHeight / 2))
                                                 + (rightDir * (nearPlaneWidth / 2));
        frustum[Frustum::Corner::NearBottomLeft] = nearPlaneMidPoint - (upDir * (nearPlaneHeight / 2))
                                                   - (rightDir * (nearPlaneWidth / 2));
        frustum[Frustum::Corner::NearBottomRight] = nearPlaneMidPoint - (upDir * (nearPlaneHeight / 2))
                                                    + (rightDir * (nearPlaneWidth / 2));
        frustum[Frustum::Corner::FarTopLeft] = farPlaneMidPoint + (upDir * (farPlaneHeight / 2))
                                               - (rightDir * (farPlaneWidth / 2));
        frustum[Frustum::Corner::FarTopRight] = farPlaneMidPoint + (upDir * (farPlaneHeight / 2))
                                                + (rightDir * (farPlaneWidth / 2));
        frustum[Frustum::Corner::FarBottomLeft] = farPlaneMidPoint - (upDir * (farPlaneHeight / 2))
                                                  - (rightDir * (farPlaneWidth / 2));
        frustum[Frustum::Corner::FarBottomRight] = farPlaneMidPoint - (upDir * (farPlaneHeight / 2))
                                                   + (rightDir * (farPlaneWidth / 2));

        return frustum;
    }
};
} // namespace renderer
