/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/handles.h"

#include <glm/glm.hpp>

#include <optional>

namespace assets
{
class Material
{
  public:
    Material();

    MaterialHandle handle() const;

    void setDiffuse(const glm::vec3& diffuse);
    void setDiffuseTextureUid(uint32_t uid);

    const glm::vec3& diffuse() const;
    const std::optional<uint32_t>& diffuseTextureUid() const;

  private:
    static uint32_t nextUid();

  private:
    MaterialHandle handle_;
    glm::vec3 diffuse_{glm::vec3{0.0f}};
    std::optional<uint32_t> diffuseTextureUid_;
};
} // namespace assets
