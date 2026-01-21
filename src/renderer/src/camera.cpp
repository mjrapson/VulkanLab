/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/camera.h"

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace renderer
{
const glm::vec3& Camera::position() const
{
    return position_;
}

const glm::vec3& Camera::front() const
{
    return front_;
}

const glm::vec3& Camera::up() const
{
    return up_;
}

float Camera::yaw() const
{
    return yaw_;
}

float Camera::roll() const
{
    return roll_;
}

float Camera::pitch() const
{
    return pitch_;
}

float Camera::aspectRatio() const
{
    return aspectRatio_;
}

void Camera::setPosition(const glm::vec3& position)
{
    position_ = position;
}

void Camera::setFront(const glm::vec3& front)
{
    front_ = front;
}

void Camera::setUp(const glm::vec3& up)
{
    up_ = up;
}

void Camera::setYaw(float yaw)
{
    yaw_ = yaw;
    normalize();
}

void Camera::setRoll(float roll)
{
    roll_ = roll;
}

void Camera::setPitch(float pitch)
{
    pitch_ = pitch;
    normalize();
}

void Camera::setAspectRatio(float aspectRatio)
{
    aspectRatio_ = aspectRatio;
}

const glm::mat4 Camera::projection() const
{
    auto projection = glm::perspective(fieldOfView_, aspectRatio_, nearPlane_, farPlane_);
    projection[1][1] *= -1.0f;

    return projection;
}

const glm::mat4 Camera::view() const
{
    return glm::lookAt(position_, position_ + front_, up_);
}

void Camera::normalize()
{
    auto front = glm::vec3{};
    front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front.y = sin(glm::radians(pitch_));
    front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));

    front_ = glm::normalize(front);
}
} // namespace renderer
