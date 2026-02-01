/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/material.h"

namespace assets
{
Material::Material()
    : handle_{nextUid()}
{
}

MaterialHandle Material::handle() const
{
    return handle_;
}

void Material::setDiffuse(const glm::vec3& diffuse)
{
    diffuse_ = diffuse;
}

void Material::setDiffuseTextureUid(uint32_t uid)
{
    diffuseTextureUid_ = uid;
}

const glm::vec3& Material::diffuse() const
{
    return diffuse_;
}

const std::optional<uint32_t>& Material::diffuseTextureUid() const
{
    return diffuseTextureUid_;
}

uint32_t Material::nextUid()
{
    static uint32_t uid = 0;
    return uid++;
}
} // namespace assets
