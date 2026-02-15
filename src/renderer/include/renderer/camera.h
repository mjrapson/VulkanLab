/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

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
};
} // namespace renderer
