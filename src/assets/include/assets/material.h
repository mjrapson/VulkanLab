/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <renderer/handles.h>

#include <glm/glm.hpp>

#include <optional>

namespace assets
{
class Image;

class Material
{
  public:
    void setDiffuseColour(const glm::vec3& colour);
    const glm::vec3& diffuseColour() const;

    void setDiffuseTexture(Image* image);
    Image* diffuseTexture() const;

    void setRenderHandle(std::optional<renderer::MaterialHandle> handle);
    std::optional<renderer::MaterialHandle> renderHandle() const;

  private:
    glm::vec3 diffuseColour_;
    Image* diffuseImage_{nullptr};
    std::optional<renderer::MaterialHandle> renderHandle_;
};
} // namespace assets
