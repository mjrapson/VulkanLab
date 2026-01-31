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

    uint32_t uid() const;

    uint32_t width() const;
    uint32_t height() const;
    const std::vector<std::byte>& data() const;

  private:
    static uint32_t nextUid();

  private:
    uint32_t uid_;
    uint32_t width_;
    uint32_t height_;
    std::vector<std::byte> data_;
};
} // namespace assets
