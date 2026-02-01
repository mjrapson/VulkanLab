/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <core/handle.h>

namespace assets
{
struct MeshHandle : core::Handle<MeshHandle>
{
};
} // namespace assets

namespace std
{
template <>
struct hash<assets::MeshHandle> : hash<core::Handle<assets::MeshHandle>>
{
};
} // namespace std
