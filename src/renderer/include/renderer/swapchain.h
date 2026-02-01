/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
struct Swapchain
{
    vk::raii::SwapchainKHR swapchain{nullptr};
    vk::Extent2D extent;
    vk::SurfaceFormatKHR surfaceFormat;
    std::vector<vk::Image> images;
    std::vector<vk::raii::ImageView> views;
};
} // namespace renderer
