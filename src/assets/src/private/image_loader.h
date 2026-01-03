/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/image.h"

#include <filesystem>

namespace assets
{
Image loadImageData(const std::filesystem::path& path);
}
