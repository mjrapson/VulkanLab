/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

namespace renderer
{
class Camera
{
  public:
    const glm::vec3& position() const;
    const glm::vec3& front() const;
    const glm::vec3& up() const;
    float yaw() const;
    float roll() const;
    float pitch() const;
    float aspectRatio() const;

    void setPosition(const glm::vec3& position);
    void setFront(const glm::vec3& front);
    void setUp(const glm::vec3& up);
    void setYaw(float yaw);
    void setRoll(float roll);
    void setPitch(float pitch);
    void setAspectRatio(float aspectRatio);

    const glm::mat4 projection() const;
    const glm::mat4 view() const;

  private:
    void normalize();

  private:
    glm::vec3 position_{0.0f, 0.0f, 0.0f};
    glm::vec3 front_{0.0f, 0.0f, -1.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};
    float fieldOfView_{45.0f};
    float nearPlane_{0.1f};
    float farPlane_{1000.0f};
    float pitch_{0.0f};
    float yaw_{0.0f};
    float roll_{0.0f};
    float aspectRatio_{1.0f};
};
} // namespace renderer
