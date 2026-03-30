/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "assets/image.h"
#include "assets/material.h"
#include "assets/mesh.h"
#include "assets/prefab.h"

#include <core/memory.h>

#include <memory>

namespace assets
{
struct ImageData;

class AssetDatabase
{
  public:
    core::Handle<Image> createImage(std::unique_ptr<ImageData> imageData);
    core::Handle<Material> createMaterial();
    core::Handle<Mesh> createMesh();
    core::Handle<Prefab> createPrefab();
    core::Handle<Image> createSkybox(std::unique_ptr<ImageData> imageData);

    Image* image(core::Handle<Image> handle);
    Material* material(core::Handle<Material> handle);
    Mesh* mesh(core::Handle<Mesh> handle);
    Prefab* prefab(core::Handle<Prefab> handle);
    Image* skybox(core::Handle<Image> handle);

  private:
    core::MemoryStore<Image> images_{64};
    core::MemoryStore<Material> materials_{128};
    core::MemoryStore<Mesh> meshes_{64};
    core::MemoryStore<Prefab> prefabs_{256};
    core::MemoryStore<Image> skyboxes_{8};
};
} // namespace assets
