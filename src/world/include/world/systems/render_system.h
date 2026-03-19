/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <memory.h>

namespace renderer
{
class Camera;
class Renderer;
} // namespace renderer

namespace world
{
class World;

class RenderSystem
{
  public:
    RenderSystem(renderer::Renderer& renderer, World& world);

    void initialize();
    void update(const renderer::Camera& camera);

  private:
    renderer::Renderer& renderer_;
    World& world_;
};
} // namespace world
