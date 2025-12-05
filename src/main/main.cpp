// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include <spdlog/spdlog.h>

#include <stdexcept>

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#else
    __attribute__((visibility("default"))) unsigned long NvOptimusEnablement = 0x00000001;
    __attribute__((visibility("default"))) int AmdPowerXpressRequestHighPerformance = 1;
#endif
}

constexpr auto windowWidth = 1200;
constexpr auto windowHeight = 900;
constexpr auto windowTitle = "Vulkan Demo";

int main(int /* argc */, char** /* argv */)
{
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::trace);

    spdlog::info("==== Vulkan Demo ====");
    spdlog::info("Build: {} {}", __DATE__, __TIME__);

    try
    {
        VulkanApplication app;
        app.init(windowWidth, windowHeight, windowTitle);
        app.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("{}", ex.what());
        return 1;
    }

    return 0;
}