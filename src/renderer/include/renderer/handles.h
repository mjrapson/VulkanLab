/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/handle.h>

namespace renderer
{
struct MaterialTag
{
};
using MaterialHandle = core::Handle<MaterialTag>;
using MaterialHandleGenerator = core::HandleGenerator<MaterialTag>;

struct MeshTag
{
};
using MeshHandle = core::Handle<MeshTag>;
using MeshHandleGenerator = core::HandleGenerator<MeshTag>;

struct ImageTag
{
};
using ImageHandle = core::Handle<ImageTag>;
using ImageHandleGenerator = core::HandleGenerator<ImageTag>;

struct SkyboxTag
{
};
using SkyboxHandle = core::Handle<SkyboxTag>;
using SkyboxHandleGenerator = core::HandleGenerator<SkyboxTag>;
} // namespace renderer
