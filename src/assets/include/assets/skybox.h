/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/image_data.h"

#include <renderer/handles.h>

#include <memory>
#include <optional>

namespace assets
{
class Image;

class Skybox
{
  public:
    explicit Skybox(std::unique_ptr<ImageData> imageData);

    const Image& image() const;

    void setRenderHandle(std::optional<renderer::SkyboxHandle> handle);
    std::optional<renderer::SkyboxHandle> renderHandle();

  private:
    std::unique_ptr<Image> image_{nullptr};
    std::optional<renderer::SkyboxHandle> renderHandle_;
};
} // namespace assets
