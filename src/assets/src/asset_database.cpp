/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/asset_database.h"

namespace assets
{
AssetHandle<Image> AssetDatabase::addImage(Image&& image)
{
    return images_.add(std::move(image));
}

AssetHandle<Material> AssetDatabase::addMaterial(Material&& material)
{
    return materials_.add(std::move(material));
}

AssetHandle<Mesh> AssetDatabase::addMesh(Mesh&& mesh)
{
    return meshes_.add(std::move(mesh));
}

AssetHandle<Prefab> AssetDatabase::addPrefab(Prefab&& prefab)
{
    return prefabs_.add(std::move(prefab));
}

std::optional<std::reference_wrapper<const Image>> AssetDatabase::getImage(const AssetHandle<Image>& handle) const
{
    return images_.get(handle);
}

std::optional<std::reference_wrapper<const Material>>
AssetDatabase::getMaterial(const AssetHandle<Material>& handle) const
{
    return materials_.get(handle);
}

std::optional<std::reference_wrapper<const Mesh>> AssetDatabase::getMesh(const AssetHandle<Mesh>& handle) const
{
    return meshes_.get(handle);
}

std::optional<std::reference_wrapper<const Prefab>> AssetDatabase::getPrefab(const AssetHandle<Prefab>& handle) const
{
    return prefabs_.get(handle);
}

const AssetStorage<Image>& AssetDatabase::images() const
{
    return images_;
}

const AssetStorage<Material>& AssetDatabase::materials() const
{
    return materials_;
}

const AssetStorage<Mesh>& AssetDatabase::meshes() const
{
    return meshes_;
}

void AssetDatabase::clear()
{
    images_.clear();
    materials_.clear();
    meshes_.clear();
    prefabs_.clear();
}
} // namespace assets
