// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <filesystem>

namespace scene
{
struct Scene;

void loadGltfScene(const std::filesystem::path& path, Scene& scene);
} // namespace scene
