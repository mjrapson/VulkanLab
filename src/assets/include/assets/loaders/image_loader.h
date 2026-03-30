/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <filesystem>
#include <memory>
#include <vector>

namespace assets
{
struct ImageData;

std::unique_ptr<ImageData> createImageFromPath(const std::filesystem::path& path);
std::unique_ptr<ImageData> createSkyboxImageFromPath(const std::filesystem::path& path);
std::unique_ptr<ImageData> createImageFromData(int width, int height, const std::vector<unsigned char>& data);
} // namespace assets
