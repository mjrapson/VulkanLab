/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/data.h"

#include <filesystem>
#include <memory>

renderer::ImageData createImageFromPath(const std::filesystem::path& path);
renderer::ImageData createImageFromData(int width, int height, const std::vector<unsigned char>& data);
