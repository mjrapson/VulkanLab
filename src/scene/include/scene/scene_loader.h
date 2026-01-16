// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <filesystem>
#include <memory>

namespace scene
{
struct Scene;
std::unique_ptr<Scene> loadScene(const std::filesystem::path& path);
}
