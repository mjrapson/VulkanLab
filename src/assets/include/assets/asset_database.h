/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "asset_handle.h"
#include "asset_storage.h"
#include "image.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace assets
{
class AssetDatabase
{
  public:
    AssetHandle<Image> loadImage(const std::filesystem::path& path);

    std::optional<std::reference_wrapper<const Image>>
    getImage(const AssetHandle<Image>& handle) const;

    void clear();

  private:
    AssetStorage<Image> images_;
};
} // namespace assets
