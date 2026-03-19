/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/loading_screen.h"

namespace assets
{
LoadingScreen::LoadingScreen(std::unique_ptr<ImageData> imageData)
    : imageData_{std::move(imageData)}
{
}

uint32_t LoadingScreen::width() const
{
    return imageData_->width;
}

uint32_t LoadingScreen::height() const
{
    return imageData_->height;
}

uint32_t LoadingScreen::components() const
{
    return imageData_->components;
}

const std::vector<std::byte>& LoadingScreen::data() const
{
    return imageData_->data;
}

void LoadingScreen::setRenderHandle(std::optional<renderer::LoadingScreenHandle> handle)
{
    renderHandle_ = handle;
}

std::optional<renderer::LoadingScreenHandle> LoadingScreen::renderHandle() const
{
    return renderHandle_;
}
} // namespace assets
