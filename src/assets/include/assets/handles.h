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

namespace std
{
template <>
struct hash<assets::MaterialHandle> : hash<core::Handle<assets::MaterialHandle>>
{
};
template <>
struct hash<assets::MeshHandle> : hash<core::Handle<assets::MeshHandle>>
{
};
template <>
struct hash<assets::SubMeshHandle> : hash<core::Handle<assets::SubMeshHandle>>
{
};
template <>
struct hash<assets::ImageHandle> : hash<core::Handle<assets::ImageHandle>>
{
};
template <>
struct hash<assets::SkyboxHandle> : hash<core::Handle<assets::SkyboxHandle>>
{
};
} // namespace std
