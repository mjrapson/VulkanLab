/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/asset_database.h"
#include "private/image_loader.h"

namespace assets
{
AssetHandle<Image> AssetDatabase::loadImage(const std::filesystem::path& path)
{
    return images_.add(std::move(loadImageData(path)));
}

std::optional<std::reference_wrapper<const Image>>
AssetDatabase::getImage(const AssetHandle<Image>& handle) const
{
    return images_.get(handle);
}

void AssetDatabase::clear()
{
    images_.clear();
}
} // namespace assets
