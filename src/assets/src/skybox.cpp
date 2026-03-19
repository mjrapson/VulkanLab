/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/skybox.h"

namespace assets
{
Skybox::Skybox(std::array<std::unique_ptr<ImageData>, 6>&& cubemapImages)
    : images_{std::move(cubemapImages)}
{
}

const std::array<std::unique_ptr<ImageData>, 6>& Skybox::images() const
{
    return images_;
}

void Skybox::setRenderHandle(std::optional<renderer::SkyboxHandle> handle)
{
    renderHandle_ = handle;
}

std::optional<renderer::SkyboxHandle> Skybox::renderHandle()
{
    return renderHandle_;
}
} // namespace assets
