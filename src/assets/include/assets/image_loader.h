/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/image.h"

#include <filesystem>
#include <memory>

namespace assets
{
std::unique_ptr<Image> createImageFromPath(const std::filesystem::path& path);
std::unique_ptr<Image> createImageFromData(int width, int height, const std::vector<unsigned char>& data);
} // namespace assets
