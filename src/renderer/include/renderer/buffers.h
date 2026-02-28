/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/buffer_object.h"

#include <vector>

namespace renderer
{
struct Buffers
{
    BufferObject meshVertexBuffer;
    BufferObject meshIndexBuffer;
    BufferObject materialBuffer;
    BufferObject directionalLightBuffer;
    std::vector<BufferObject> cameraBuffers;
};
} // namespace renderer
