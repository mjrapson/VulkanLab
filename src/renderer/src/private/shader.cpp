/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "shader.h"

namespace renderer
{
[[nodiscard]] vk::raii::ShaderModule createShaderModule(const vk::raii::Device& device,
                                                        const std::vector<char>& code)
{
    const auto createInfo = vk::ShaderModuleCreateInfo{
        .codeSize = code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    };

    return vk::raii::ShaderModule{device, createInfo};
}
} // namespace renderer