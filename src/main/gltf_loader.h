// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <filesystem>
#include <memory>

namespace renderer
{
struct AssetData;
}

namespace world
{
class Prefab;
}

std::unique_ptr<world::Prefab> loadGLTFModel(const std::filesystem::path& path, renderer::AssetData& assetData);
