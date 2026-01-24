/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/image_loader.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <cstring>

namespace assets
{
std::unique_ptr<Image> createImageFromPath(const std::filesystem::path& path)
{
    int width;
    int height;
    int channels;

    stbi_set_flip_vertically_on_load(true);
    auto stbiData = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!stbiData)
    {
        throw std::runtime_error("Failed to load image: " + path.string());
    }

    const auto imageSize = static_cast<size_t>(width) * height * STBI_rgb_alpha;

    auto data = std::vector<std::byte>(imageSize);
    std::memcpy(data.data(), stbiData, imageSize);

    stbi_image_free(stbiData);

    return std::make_unique<Image>(static_cast<uint32_t>(width), static_cast<uint32_t>(height), std::move(data));
}

std::unique_ptr<Image> createImageFromData(int width, int height, const std::vector<unsigned char>& data)
{
    auto byteData = std::vector<std::byte>(data.size());
    std::memcpy(byteData.data(), data.data(), data.size());

    return std::make_unique<Image>(static_cast<uint32_t>(width), static_cast<uint32_t>(height), std::move(byteData));
}
} // namespace assets
