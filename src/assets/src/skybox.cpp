/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/skybox.h"

#include "assets/image.h"

namespace assets
{
Skybox::Skybox(std::unique_ptr<ImageData> imageData)
    : image_{std::make_unique<Image>(std::move(imageData))}
{
}

const Image& Skybox::image() const
{
    return *image_;
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
