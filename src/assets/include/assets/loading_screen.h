/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/image_data.h"

#include <renderer/handles.h>

#include <optional>
#include <stdint.h>
#include <vector>

namespace assets
{
class LoadingScreen
{
  public:
    explicit LoadingScreen(std::unique_ptr<ImageData> imageData);

    uint32_t width() const;
    uint32_t height() const;
    uint32_t components() const;
    const std::vector<std::byte>& data() const;

    void setRenderHandle(std::optional<renderer::LoadingScreenHandle> handle);
    std::optional<renderer::LoadingScreenHandle> renderHandle() const;

  private:
    std::unique_ptr<ImageData> imageData_;
    std::optional<renderer::LoadingScreenHandle> renderHandle_;
};
} // namespace assets
