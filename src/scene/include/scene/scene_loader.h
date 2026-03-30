/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "scene/scene.h"

#include <filesystem>
#include <memory>

namespace scene
{
std::unique_ptr<Scene> loadSceneFromFolder(const std::filesystem::path& path);
}
