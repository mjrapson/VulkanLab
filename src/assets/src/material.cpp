/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/material.h"

namespace assets
{
void Material::setDiffuseColour(const glm::vec3& colour)
{
    diffuseColour_ = colour;
}

const glm::vec3& Material::diffuseColour() const
{
    return diffuseColour_;
}

void Material::setDiffuseTexture(core::Handle<Image> image)
{
    diffuseImage_ = image;
}

std::optional<core::Handle<Image>> Material::diffuseTexture() const
{
    return diffuseImage_;
}

void Material::setRenderHandle(std::optional<renderer::MaterialHandle> handle)
{
    renderHandle_ = handle;
}

std::optional<renderer::MaterialHandle> Material::renderHandle() const
{
    return renderHandle_;
}
} // namespace assets
