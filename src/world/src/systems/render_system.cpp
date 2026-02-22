/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/systems/render_system.h"

#include <renderer/camera.h>
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
    auto drawInfo = renderer::Renderer::SceneDrawInfo{};
    drawInfo.skyboxHandle = world_.activeSkybox();
    drawInfo.globalLightDirection = world_.globalLightDirection();

    for (auto& [entity, renderComponent] : world_.getAllComponents<RenderComponent>())
    {
        auto prefab = renderComponent.prefab;
        if (!prefab || prefab->meshInstances.empty())
        {
            continue;
        }

        auto transformComponent = world_.getComponent<TransformComponent>(entity);
        assert(transformComponent && "Entity with render component missing transform");

        const auto transformMatrix = glm::translate(glm::mat4(1.0f), transformComponent->position)
                                     * glm::toMat4(glm::quat(glm::radians(transformComponent->rotation)))
                                     * glm::scale(glm::mat4(1.0f), transformComponent->scale);

        for (const auto& instance : prefab->meshInstances)
        {

            for (const auto& meshHandle : instance.subMeshes)
            {
                drawInfo.drawCommands.push_back(
                    renderer::DrawCommand{meshHandle, transformMatrix * instance.transform});
            }
        }
    }

    renderer_.renderScene(camera, drawInfo);
}
} // namespace world
