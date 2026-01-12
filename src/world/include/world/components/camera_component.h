/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <renderer/camera.h>

#include <glm/glm.hpp>

namespace world
{
class CameraComponent
{
  public:
    const Camera& camera() const;
    const glm::vec3& position() const;
    const glm::vec3& front() const;
    const glm::vec3& up() const;
    float yaw() const;
    float roll() const;
    float pitch() const;

    void setPosition(const glm::vec3& position);
    void setFront(const glm::vec3& front);
    void setUp(const glm::vec3& up);
    void setYaw(float yaw);
    void setRoll(float roll);
    void setPitch(float pitch);

  private:
    void normalize();

  private:
    Camera camera_;
};
} // namespace world
