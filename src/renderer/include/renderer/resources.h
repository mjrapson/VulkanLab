/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/gpu_objects.h"

namespace renderer
{
struct Resources
{
    Image emptyImage;
    MeshContainer meshes;
    MaterialContainer materials;
    ImageContainer images;
    SkyboxContainer skyboxes;
    ImageContainer loadingScreens;
};
} // namespace renderer
