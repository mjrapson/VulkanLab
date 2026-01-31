/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <glm/glm.hpp>

#include <optional>

namespace assets
{
class Material
{
  public:
    Material();

    uint32_t uid() const;

    void setDiffuse(const glm::vec3& diffuse);
    void setDiffuseTextureUid(uint32_t uid);

    const glm::vec3& diffuse() const;
    const std::optional<uint32_t>& diffuseTextureUid() const;

  private:
    static uint32_t nextUid();

  private:
    uint32_t uid_;
    glm::vec3 diffuse_{glm::vec3{0.0f}};
    std::optional<uint32_t> diffuseTextureUid_;
};
} // namespace assets
