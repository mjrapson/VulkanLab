/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/components/camera_component.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace world
{
const Camera& CameraComponent::camera() const
{
    return camera_;
}

const glm::vec3& CameraComponent::position() const
{
    return camera_.position;
}

const glm::vec3& CameraComponent::front() const
{
    return camera_.front;
}

const glm::vec3& CameraComponent::up() const
{
    return camera_.up;
}

float CameraComponent::yaw() const
{
    return camera_.yaw;
}

float CameraComponent::roll() const
{
    return camera_.roll;
}

float CameraComponent::pitch() const
{
    return camera_.pitch;
}

void CameraComponent::setPosition(const glm::vec3& position)
{
    camera_.position = position;
}

void CameraComponent::setFront(const glm::vec3& front)
{
    camera_.front = front;
}

void CameraComponent::setUp(const glm::vec3& up)
{
    camera_.up = up;
}

void CameraComponent::setYaw(float yaw)
{
    camera_.yaw = yaw;
    normalize();
}

void CameraComponent::setRoll(float roll)
{
    camera_.roll = roll;
}

void CameraComponent::setPitch(float pitch)
{
    camera_.pitch = pitch;
    normalize();
}

void CameraComponent::normalize()
{
    glm::vec3 front;
    front.x = cos(glm::radians(camera_.yaw)) * cos(glm::radians(camera_.pitch));
    front.y = sin(glm::radians(camera_.pitch));
    front.z = sin(glm::radians(camera_.yaw)) * cos(glm::radians(camera_.pitch));

    camera_.front = glm::normalize(front);
}
} // namespace world
