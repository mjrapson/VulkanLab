/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "window/window.h"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace window
{
static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto winptr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    winptr->windowResized(width, height);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto winptr = static_cast<Window*>(glfwGetWindowUserPointer(window));
    winptr->keyPressed(key, scancode, action, mods);
}

Window::Window(int width, int height, std::string_view title)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
    if (!window_)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwGetWindowSize(window_, &width_, &height_);

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetKeyCallback(window_, keyCallback);
}

Window::~Window()
{
    glfwDestroyWindow(window_);
    window_ = nullptr;
}

void Window::windowResized(int width, int height)
{
    resized_ = true;
    width_ = width;
    height_ = height;
}

void Window::keyPressed(int key, int, int action, int)
{
    if (action == GLFW_PRESS)
    {
        inputHandler_.setKeyPressed(key);
    }
    else if (action == GLFW_RELEASE)
    {
        inputHandler_.setKeyReleased(key);
    }
}

std::span<const char* const> Window::requiredExtensions() const
{
    auto count = uint32_t{0};
    const auto extensions = glfwGetRequiredInstanceExtensions(&count);

    return std::span<const char* const>{extensions, count};
}

vk::raii::SurfaceKHR Window::createVulkanSurface(const vk::raii::Instance& instance) const
{
    auto surface = VkSurfaceKHR{VK_NULL_HANDLE};
    if (glfwCreateWindowSurface(*instance, window_, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface");
    }

    return vk::raii::SurfaceKHR{instance, surface};
}

void Window::pollEvents()
{
    resized_ = false;

    glfwPollEvents();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(window_);
}

bool Window::resized() const
{
    return resized_;
}

int Window::width() const
{
    return width_;
}

int Window::height() const
{
    return height_;
}

const core::InputHandler& Window::inputHandler() const
{
    return inputHandler_;
}
} // namespace window
