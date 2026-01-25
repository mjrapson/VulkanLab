/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/systems/render_system.h"

#include <assets/asset_database.h>
#include <renderer/renderer.h>

#include "world/world.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace world
{
RenderSystem::RenderSystem(renderer::Renderer& renderer, World& world)
    : renderer_{renderer},
      world_{world}
{
}

void RenderSystem::update(const renderer::Camera& camera)
{
    auto commands = std::vector<renderer::DrawCommand>{};
    for (auto& [entity, renderComponent] : world_.getAllComponents<RenderComponent>())
    {
        auto prefab = renderComponent.prefab;
        if (!prefab)
        {
            continue;
        }

        if (prefab->meshes().empty())
        {
            continue;
        }

        auto transformComponent = world_.getComponent<TransformComponent>(entity);
        if (!transformComponent)
        {
            continue;
        }

        auto transformMatrix = glm::translate(glm::mat4(1.0f), transformComponent->position)
                               * glm::toMat4(glm::quat(glm::radians(transformComponent->rotation)))
                               * glm::scale(glm::mat4(1.0f), transformComponent->scale);

        for (const auto& instance : prefab->meshInstances())
        {
            if (!instance.mesh)
            {
                continue;
            }

            for (const auto& subMesh : instance.mesh->subMeshes)
            {
                auto drawCommand = renderer::DrawCommand{};
                drawCommand.subMesh = subMesh.get();
                drawCommand.transform = transformMatrix * instance.transform;
                commands.push_back(drawCommand);
            }
        }
    }

    renderer_.renderFrame(camera, world_.activeSkybox(), commands);
}
} // namespace world
