/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/image_data.h"

#include <renderer/handles.h>

#include <array>
#include <memory>
#include <optional>

namespace assets
{
class Skybox
{
  public:
    explicit Skybox(std::array<std::unique_ptr<ImageData>, 6>&& cubemapImages);

    const std::array<std::unique_ptr<ImageData>, 6>& images() const;

    void setRenderHandle(std::optional<renderer::SkyboxHandle> handle);
    std::optional<renderer::SkyboxHandle> renderHandle();

  private:
    std::array<std::unique_ptr<ImageData>, 6> images_;
    std::optional<renderer::SkyboxHandle> renderHandle_;
};
} // namespace assets
