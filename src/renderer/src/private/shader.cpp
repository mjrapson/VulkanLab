/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "shader.h"

namespace renderer
{
[[nodiscard]] vk::raii::ShaderModule createShaderModule(const vk::raii::Device& device,
                                                        const std::vector<char>& code) noexcept
{

    auto createInfo = vk::ShaderModuleCreateInfo{};
    createInfo.codeSize = code.size() * sizeof(char);
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    return vk::raii::ShaderModule{device, createInfo};
}
} // namespace renderer
