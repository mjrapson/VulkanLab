/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <stdint.h>
#include <vector>

namespace assets
{
class Image
{
  public:
    Image(uint32_t width, uint32_t height, std::vector<std::byte>&& data);
    ~Image() = default;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other) = default;
    Image& operator=(Image&& other) = default;

    uint32_t width() const;
    uint32_t height() const;

    const std::vector<std::byte>& data() const;

  private:
    uint32_t width_;
    uint32_t height_;
    std::vector<std::byte> data_;
};
} // namespace assets
