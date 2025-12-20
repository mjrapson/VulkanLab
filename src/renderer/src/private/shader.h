/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
[[nodiscard]]
vk::raii::ShaderModule createShaderModule(const vk::raii::Device& device,
                                          const std::vector<char>& code);
}