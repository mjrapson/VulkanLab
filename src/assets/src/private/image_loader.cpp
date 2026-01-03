/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>

namespace assets
{
Image loadImageData(const std::filesystem::path& path)
{
    int width;
    int height;
    int channels;

    auto stbiData = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!stbiData)
    {
        throw std::runtime_error("Failed to load image: " + path.string());
    }

    const auto imageSize = static_cast<size_t>(width) * height * STBI_rgb_alpha;

    auto data = std::vector<std::byte>(imageSize);
    std::memcpy(data.data(), stbiData, imageSize);

    stbi_image_free(stbiData);

    return Image(width, height, std::move(data));
}
} // namespace assets
