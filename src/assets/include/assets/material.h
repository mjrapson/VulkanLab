/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "asset_handle.h"
#include "image.h"

#include <glm/glm.hpp>

#include <optional>

namespace assets
{
struct Material
{
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    std::optional<AssetHandle<Image>> diffuseTexture;
};
} // namespace assets
