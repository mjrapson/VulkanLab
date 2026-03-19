/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/image.h"

namespace assets
{
Image::Image(std::unique_ptr<ImageData> imageData)
    : imageData_{std::move(imageData)}
{
}

uint32_t Image::width() const
{
    return imageData_->width;
}

uint32_t Image::height() const
{
    return imageData_->height;
}

uint32_t Image::components() const
{
    return imageData_->components;
}

const std::vector<std::byte>& Image::data() const
{
    return imageData_->data;
}

void Image::setRenderHandle(std::optional<renderer::ImageHandle> handle)
{
    renderHandle_ = handle;
}

std::optional<renderer::ImageHandle> Image::renderHandle() const
{
    return renderHandle_;
}
} // namespace assets
