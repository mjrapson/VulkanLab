/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/handle.h>

#include "renderer/gpu_objects.h"

namespace renderer
{
using MaterialHandle = core::Handle<Material>;
using MeshHandle = core::Handle<Mesh>;
using ImageHandle = core::Handle<Image>;
using SkyboxHandle = core::Handle<Skybox>;
using LoadingScreenHandle = core::Handle<LoadingScreen>;
} // namespace renderer
