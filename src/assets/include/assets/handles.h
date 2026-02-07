/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/handle.h>

namespace assets
{
struct MaterialHandle : core::Handle<MaterialHandle>
{
};
struct MeshHandle : core::Handle<MeshHandle>
{
};
struct SubMeshHandle : core::Handle<SubMeshHandle>
{
};
struct ImageHandle : core::Handle<ImageHandle>
{
};
struct SkyboxHandle : core::Handle<SkyboxHandle>
{
};
} // namespace assets
