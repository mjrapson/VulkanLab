/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "image.h"
#include "material.h"
#include "mesh.h"
#include "mesh_instance.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace assets
{
class Prefab
{
  public:
    void addMaterial(const std::string& name, std::unique_ptr<Material> material);
    void addMesh(std::unique_ptr<Mesh> mesh);
    void addImage(const std::string& name, std::unique_ptr<Image> image);

    void addMeshInstance(MeshInstance&& instance);

    Material* getMaterial(const std::string& name) const;
    Mesh* getMesh(int index) const;
    Image* getImage(const std::string& name) const;

    const std::unordered_map<std::string, std::unique_ptr<Material>>& materials() const;
    const std::vector<std::unique_ptr<Mesh>>& meshes() const;
    const std::unordered_map<std::string, std::unique_ptr<Image>>& images() const;

    const std::vector<MeshInstance>& meshInstances() const;

  private:
    std::unordered_map<std::string, std::unique_ptr<Material>> materials_;
    std::vector<std::unique_ptr<Mesh>> meshes_;
    std::unordered_map<std::string, std::unique_ptr<Image>> images_;
    std::vector<MeshInstance> meshInstances_;
};
} // namespace assets
