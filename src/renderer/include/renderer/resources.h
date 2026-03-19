/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/gpu_objects.h"
#include "renderer/handles.h"

#include <core/memory.h>

namespace renderer
{
struct Resources
{
    ImageHandle emptyImage;
    core::MemoryStore<Mesh> meshes{64};
    core::MemoryStore<Material> materials{128};
    core::MemoryStore<Image> images{64};
    core::MemoryStore<Skybox> skyboxes{32};
    core::MemoryStore<LoadingScreen> loadingScreens{32};
};
} // namespace renderer
