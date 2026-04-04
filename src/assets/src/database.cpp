/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/database.h"

namespace assets
{
core::Handle<Image> AssetDatabase::createImage(std::unique_ptr<ImageData> imageData)
{
    return images_.allocate(std::move(imageData));
}

core::Handle<Material> AssetDatabase::createMaterial()
{
    return materials_.allocate();
}

core::Handle<Mesh> AssetDatabase::createMesh()
{
    return meshes_.allocate();
}

core::Handle<Prefab> AssetDatabase::createPrefab()
{
    return prefabs_.allocate();
}

core::Handle<Skybox> AssetDatabase::createSkybox(std::unique_ptr<ImageData> imageData)
{
    return skyboxes_.allocate(std::move(imageData));
}

Image* AssetDatabase::image(core::Handle<Image> handle)
{
    return images_.get(handle);
}

Material* AssetDatabase::material(core::Handle<Material> handle)
{
    return materials_.get(handle);
}

Mesh* AssetDatabase::mesh(core::Handle<Mesh> handle)
{
    return meshes_.get(handle);
}

Prefab* AssetDatabase::prefab(core::Handle<Prefab> handle)
{
    return prefabs_.get(handle);
}

Skybox* AssetDatabase::skybox(core::Handle<Skybox> handle)
{
    return skyboxes_.get(handle);
}
} // namespace assets
