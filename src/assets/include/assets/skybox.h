/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/handles.h"

#include <array>
#include <memory>

namespace assets
{
struct Image;

class Skybox
{
  public:
    Skybox();

    uint32_t uid() const;

    uint32_t width() const;
    uint32_t height() const;
    uint32_t faceCount() const;

    void setImage(int face, std::unique_ptr<Image> image);
    Image* imageAt(int face) const;

    static uint32_t nextUid();

  private:
    uint32_t uid_;
    std::array<std::unique_ptr<Image>, 6> images_;
};
} // namespace assets
