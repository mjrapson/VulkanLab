/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "world/systems/render_system.h"

#include "assets/skybox.h"
#include "world/world.h"

#include <renderer/camera.h>
#include <renderer/renderer.h>

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

void RenderSystem::initialize()
{
    renderer_.reset();

    for (const auto& prefab : world_.prefabs())
    {
        for (auto& image : prefab->images)
        {
            image->setRenderHandle(renderer_.addImage(image->width(), image->height(), image->data()));
        }

        for (auto& material : prefab->materials)
        {
            auto materialData = renderer::Renderer::MaterialData{};
            materialData.diffuseColor = material->diffuseColour();
            if (material->diffuseTexture())
            {
                materialData.diffuseTexture = material->diffuseTexture()->renderHandle();
            }

            material->setRenderHandle(renderer_.addMaterial(materialData));
        }

        for (auto& mesh : prefab->meshes)
        {
            mesh->setRenderHandle(renderer_.addMesh(mesh->vertices(), mesh->indices()));
        }
    }

    for (auto& skybox : world_.skyboxes())
    {
        auto data = std::array<renderer::Renderer::FaceData, 6>{};
        for (auto i = size_t{0}; i < skybox->images().size(); ++i)
        {
            auto faceData = renderer::Renderer::FaceData{};
            faceData.width = skybox->images().at(i)->width;
            faceData.height = skybox->images().at(i)->height;
            faceData.data = skybox->images().at(i)->data;

            data[i] = faceData;
        }

        skybox->setRenderHandle(renderer_.addSkybox(data));
    }
}

void RenderSystem::update(const renderer::Camera& camera)
{
    auto drawInfo = renderer::Renderer::SceneDrawInfo{};
    if (world_.environment().skybox)
    {
        drawInfo.skyboxHandle = world_.environment().skybox->renderHandle();
    }
    drawInfo.globalLightDirection = world_.environment().directionalLight.direction;

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
            const auto worldTransform = transformMatrix * instance.transform;

            for (const auto& mesh : instance.subMeshes)
            {
                if (!mesh || !mesh->renderHandle())
                {
                    continue;
                }

                if (!mesh->material() || !mesh->material()->renderHandle())
                {
                    continue;
                }

                drawInfo.drawCommands.push_back(
                    renderer::DrawCommand{*mesh->renderHandle(), *mesh->material()->renderHandle(), worldTransform});
            }
        }
    }

    renderer_.renderScene(camera, drawInfo);
}
} // namespace world
