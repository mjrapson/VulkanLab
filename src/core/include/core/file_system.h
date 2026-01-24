// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <filesystem>
#include <vector>

namespace core
{
std::filesystem::path getRootDir();
std::filesystem::path getScenesDir();
std::filesystem::path getShaderDir();
std::filesystem::path getPrefabsDir();
std::filesystem::path getTexturesDir();
std::filesystem::path getSkyboxesDir();

[[nodiscard]]
std::vector<char> readBinaryFile(const std::filesystem::path& filepath);
}
