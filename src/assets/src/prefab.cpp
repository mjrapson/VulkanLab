/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/prefab.h"

namespace assets
{
void Prefab::addMaterial(const std::string& name, std::unique_ptr<Material> material)
{
    if (!material)
    {
        return;
    }

    materials_[name] = std::move(material);
}

void Prefab::addMesh(std::unique_ptr<Mesh> mesh)
{
    if (!mesh)
    {
        return;
    }

    meshes_.push_back(std::move(mesh));
}

void Prefab::addImage(const std::string& name, std::unique_ptr<Image> image)
{
    if (!image)
    {
        return;
    }

    images_[name] = std::move(image);
}

Material* Prefab::getMaterial(const std::string& name) const
{
    if (!materials_.contains(name))
    {
        return nullptr;
    }

    return materials_.at(name).get();
}

Image* Prefab::getImage(const std::string& name) const
{
    if (!images_.contains(name))
    {
        return nullptr;
    }

    return images_.at(name).get();
}

const std::unordered_map<std::string, std::unique_ptr<Material>>& Prefab::materials() const
{
    return materials_;
}

const std::vector<std::unique_ptr<Mesh>>& Prefab::meshes() const
{
    return meshes_;
}

const std::unordered_map<std::string, std::unique_ptr<Image>>& Prefab::images() const
{
    return images_;
}
} // namespace assets
