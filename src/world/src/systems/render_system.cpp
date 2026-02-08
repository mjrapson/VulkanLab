/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/systems/render_system.h"

#include <assets/asset_database.h>
#include <renderer/gpu_resource_cache.h>
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

std::future<std::unique_ptr<renderer::GpuResourceCache>>
RenderSystem::initialize(const assets::AssetDatabase& assetDatabase) const
{
    return std::async(std::launch::async,
                      [this, &assetDatabase]
                      {
                          return std::make_unique<renderer::GpuResourceCache>(assetDatabase, renderer_.device());
                      });
}

void RenderSystem::update(const renderer::Camera& camera)
{
    auto meshDrawCommands = std::vector<renderer::DrawCommand>{};

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

            auto mesh = prefab->mesh(instance.meshHandle);
            if (!mesh)
            {
                continue;
            }

            for (const auto& subMesh : mesh->subMeshes())
            {
                auto cmd = renderer::DrawCommand{};
                cmd.materialHandle = subMesh.materialHandle;
                cmd.subMeshHandle = subMesh.handle;
                cmd.transform = transformMatrix * instance.transform;

                meshDrawCommands.push_back(cmd);
            }
        }
    }

    renderer_.renderScene(meshDrawCommands, world_.activeSkybox(), camera);
}
} // namespace world
