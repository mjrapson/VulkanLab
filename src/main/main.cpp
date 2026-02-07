// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include <renderer/renderer.h>
#include <window/window.h>

#include <spdlog/spdlog.h>

#include <GLFW/glfw3.h>

#include <stdexcept>

constexpr auto windowWidth = 1440;
constexpr auto windowHeight = 1080;
constexpr auto windowTitle = "Vulkan Demo";

int main(int /* argc */, char** /* argv */)
{
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::trace);

    spdlog::info("==== Vulkan Demo ====");
    spdlog::info("Build: {} {}", __DATE__, __TIME__);

    glfwSetErrorCallback(
        [](int errorCode, const char* description)
        {
            spdlog::error("GLFW error {}: {}", errorCode, description);
        });

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    try
    {
        auto window = window::Window{windowWidth, windowHeight, windowTitle};
        auto renderer = renderer::Renderer{window};

        auto app = VulkanApplication{window, renderer};
        app.run();
    }
    catch (const std::exception& ex)
    {
        glfwTerminate();
        spdlog::critical("{}", ex.what());
        return 1;
    }

    return 0;
}
