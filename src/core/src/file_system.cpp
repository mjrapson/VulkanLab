// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "core/file_system.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <linux/limits.h>
#include <unistd.h>
#endif

#include <fstream>

namespace core
{
std::filesystem::path getRootDir()
{
#ifdef _WIN32
    HMODULE hModule = GetModuleHandleW(NULL);
    WCHAR path[MAX_PATH];

    GetModuleFileNameW(hModule, path, MAX_PATH);
    char tempStr[MAX_PATH];
    char defChar = ' ';

    WideCharToMultiByte(CP_ACP, 0, path, -1, tempStr, MAX_PATH, &defChar, NULL);

    return std::filesystem::path{tempStr}.parent_path();
#else
    char path[PATH_MAX] = {0};
    if (readlink("/proc/self/exe", path, PATH_MAX) != -1)
    {
        return std::filesystem::path{path}.parent_path();
    }
    else
    {
        return std::filesystem::path{};
    }
#endif
}

std::filesystem::path getShaderDir()
{
    return getRootDir() / "shaders";
}

std::vector<char> readBinaryFile(const std::filesystem::path& filepath)
{
    auto file = std::ifstream(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    return buffer;
}
}