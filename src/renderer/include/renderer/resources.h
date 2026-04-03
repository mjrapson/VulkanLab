/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/buffer.h"
#include "renderer/gpu_objects.h"
#include "renderer/image.h"

#include <core/memory.h>

namespace renderer
{
struct Resources
{
    core::MemoryStore<Buffer> buffers{128};
    core::MemoryStore<Mesh> meshes{64};
    core::MemoryStore<Material> materials{128};
    core::MemoryStore<Image> images{64};
    core::MemoryStore<Skybox> skyboxes{32};
    core::MemoryStore<LoadingScreen> loadingScreens{32};
};
} // namespace renderer
