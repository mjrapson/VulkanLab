// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "core/file_system.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <linux/limits.h>
#include <unistd.h>
#endif

#include <vector>

std::filesystem::path GetRootDir()
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

std::filesystem::path GetShaderDir()
{
    return GetRootDir() / "shaders";
}