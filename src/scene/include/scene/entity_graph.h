/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "scene/entity.h"

namespace scene
{
class EntityGraph
{
  public:
    void appendEntity(Entity&& entity)
    {
        entities_.push_back(entity);
    }

    template <typename Func>
    void visit(Func&& func)
    {
        for (auto& entity : entities_)
        {
            visit(entity, func);
        }
    }

  private:
    template <typename Func>
    void visit(Entity& entity, Func&& func)
    {
        func(entity);

        for (auto& child : entity.children)
        {
            visit(child, func);
        }
    }

  private:
    std::vector<Entity> entities_;
};
} // namespace scene
