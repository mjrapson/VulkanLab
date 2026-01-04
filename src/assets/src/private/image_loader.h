/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/image.h"

#include <filesystem>

namespace assets
{
Image createImageFromPath(const std::filesystem::path& path);
Image createImageFromData(int width, int height, const std::vector<unsigned char>& data);
} // namespace assets
