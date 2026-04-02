/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/loaders/image_loader.h"

#include "assets/image_data.h"

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
std::unique_ptr<ImageData> createImageFromPath(const std::filesystem::path& path)
{
    int width;
    int height;
    int channels;

    stbi_set_flip_vertically_on_load(true);
    auto stbiData = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    stbi_set_flip_vertically_on_load(false);
    if (!stbiData)
    {
        throw std::runtime_error("Failed to load image: " + path.string());
    }

    auto imageData = std::make_unique<ImageData>();
    imageData->width = static_cast<uint32_t>(width);
    imageData->height = static_cast<uint32_t>(height);
    imageData->components = static_cast<uint32_t>(STBI_rgb_alpha);

    const auto imageSize = width * height * STBI_rgb_alpha;
    imageData->data = std::vector<std::byte>(static_cast<size_t>(imageSize));
    std::memcpy(imageData->data.data(), stbiData, static_cast<size_t>(imageSize));

    stbi_image_free(stbiData);

    return imageData;
}

std::unique_ptr<ImageData> createSkyboxImageFromPath(const std::filesystem::path& path)
{
    int width;
    int height;
    int channels;

    stbi_set_flip_vertically_on_load(true);
    auto stbiData = stbi_loadf(path.string().c_str(), &width, &height, &channels, 0);
    stbi_set_flip_vertically_on_load(false);
    if (!stbiData)
    {
        throw std::runtime_error("Failed to load image: " + path.string());
    }

    auto imageData = std::make_unique<ImageData>();
    imageData->width = static_cast<uint32_t>(width);
    imageData->height = static_cast<uint32_t>(height);
    imageData->components = static_cast<uint32_t>(STBI_rgb_alpha);

    const auto imageSize = width * height * STBI_rgb_alpha;
    imageData->data = std::vector<std::byte>(static_cast<size_t>(imageSize));
    std::memcpy(imageData->data.data(), stbiData, static_cast<size_t>(imageSize));

    stbi_image_free(stbiData);

    return imageData;
}

std::unique_ptr<ImageData> createImageFromData(int width, int height, const std::vector<unsigned char>& data)
{
    auto imageData = std::make_unique<ImageData>();
    imageData->width = static_cast<uint32_t>(width);
    imageData->height = static_cast<uint32_t>(height);
    imageData->components = static_cast<uint32_t>(STBI_rgb_alpha);
    imageData->data = std::vector<std::byte>(data.size());

    std::memcpy(imageData->data.data(), data.data(), data.size());

    return imageData;
}
} // namespace assets
