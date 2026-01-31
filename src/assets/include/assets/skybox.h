/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <memory>

namespace assets
{
struct Image;

struct Skybox
{
    Skybox()
        : uid{nextUid()}
    {
    }

    uint32_t uid;
    std::array<std::unique_ptr<Image>, 6> images;

  private:
    static uint32_t nextUid()
    {
        static uint32_t nextUid = 0;
        return nextUid++;
    }
};
} // namespace assets
