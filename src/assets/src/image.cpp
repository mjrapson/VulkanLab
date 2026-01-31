/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/image.h"

namespace assets
{
Image::Image(uint32_t width, uint32_t height, std::vector<std::byte>&& data)
    : uid_{nextUid()},
      width_{width},
      height_{height},
      data_{std::move(data)}
{
}

uint32_t Image::uid() const
{
    return uid_;
}

uint32_t Image::width() const
{
    return width_;
}

uint32_t Image::height() const
{
    return height_;
}

const std::vector<std::byte>& Image::data() const
{
    return data_;
}

uint32_t Image::nextUid()
{
    static uint32_t uid = 0;
    return uid++;
}
} // namespace assets
