/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "scene/entity_graph.h"

#include <assets/database.h>

#include <glm/glm.hpp>

namespace scene
{
struct Scene
{
    assets::AssetDatabase assetDatabase;
    EntityGraph entityGraph;
};
} // namespace scene
