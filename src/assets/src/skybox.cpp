/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "assets/skybox.h"

#include "assets/image.h"

#include <stdexcept>

namespace assets
{
Skybox::Skybox()
    : handle_{nextUid()}
{
}

SkyboxHandle Skybox::handle() const
{
    return handle_;
}

uint32_t Skybox::width() const
{
    if (!images_.at(0))
    {
        return 0;
    }

    return images_.at(0)->width();
}

uint32_t Skybox::height() const
{
    if (!images_.at(0))
    {
        return 0;
    }

    return images_.at(0)->height();
}

uint32_t Skybox::faceCount() const
{
    return static_cast<uint32_t>(images_.size());
}

void Skybox::setImage(int face, std::unique_ptr<Image> image)
{
    if(static_cast<size_t>(face) > images_.size())
    {
        throw std::out_of_range{"Face out of range of skybox cubemap array"};
    }

    images_[face] = std::move(image);
}

Image* Skybox::imageAt(int face) const
{
    if(static_cast<size_t>(face) > images_.size())
    {
        throw std::out_of_range{"Face out of range of skybox cubemap array"};
    }

    return images_.at(face).get();
}

uint32_t Skybox::nextUid()
{
    static uint32_t nextUid = 0;
    return nextUid++;
}
} // namespace assets
