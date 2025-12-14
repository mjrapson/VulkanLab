// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <filesystem>
#include <vector>

namespace core
{
std::filesystem::path getRootDir();
std::filesystem::path getShaderDir();

[[nodiscard]]
std::vector<char> readBinaryFile(const std::filesystem::path& filepath);
}