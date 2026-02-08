/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <future>
#include <memory.h>

namespace assets
{
class AssetDatabase;
} // namespace assets

namespace renderer
{
class Camera;
class GpuResourceCache;
class Renderer;
} // namespace renderer

namespace world
{
class World;

class RenderSystem
{
  public:
    RenderSystem(renderer::Renderer& renderer, World& world);

    std::future<std::unique_ptr<renderer::GpuResourceCache>>
    initialize(const assets::AssetDatabase& assetDatabase) const;

    void update(const renderer::Camera& camera);

  private:
    renderer::Renderer& renderer_;
    World& world_;
};
} // namespace world
