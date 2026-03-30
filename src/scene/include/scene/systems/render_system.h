/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <renderer/renderer.h>

#include <memory.h>

namespace assets
{
class AssetDatabase;
} // namespace assets

namespace scene
{
struct Entity;
struct Scene;

class RenderSystem
{
  public:
    explicit RenderSystem(renderer::Renderer& renderer);

    void initialize(Scene& scene);
    void update(Scene& scene, const renderer::Camera& camera);

  private:
    void initEntity(const Entity& entity, assets::AssetDatabase& database);
    void drawEntity(const Entity& entity, assets::AssetDatabase& database, renderer::Renderer::SceneDrawInfo& drawInfo);

  private:
    renderer::Renderer& renderer_;
};
} // namespace scene
