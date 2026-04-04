/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "scene/systems/render_system.h"

#include "scene/scene.h"

#include <assets/database.h>
#include <assets/skybox.h>
#include <renderer/camera.h>
#include <renderer/renderer.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace scene
{
RenderSystem::RenderSystem(renderer::Renderer& renderer)
    : renderer_{renderer}
{
}

void RenderSystem::initialize(scene::Scene& scene)
{
    renderer_.reset();

    if (scene.environment.skybox)
    {
        auto skybox = scene.assetDatabase.skybox(*scene.environment.skybox);
        if (skybox && !skybox->renderHandle())
        {
            const auto& skyboxImage = skybox->image();
            skybox->setRenderHandle(renderer_.addSkybox(skyboxImage.width(), skyboxImage.height(), skyboxImage.data()));
        }
    }

    scene.entityGraph.visit(
        [&database = scene.assetDatabase, this](Entity& entity)
        {
            if (!entity.prefabHandle)
            {
                return;
            }
            auto prefab = database.prefab(*entity.prefabHandle);
            if (!prefab)
            {
                return;
            }
            for (const auto& meshInstance : prefab->meshInstances)
            {
                auto mesh = database.mesh(meshInstance.meshHandle);

                if (!mesh)
                {
                    continue;
                }

                if (!mesh->renderHandle())
                {
                    mesh->setRenderHandle(renderer_.addMesh(mesh->vertices(), mesh->indices()));
                }

                if (meshInstance.materialHandle)
                {
                    auto material = database.material(*meshInstance.materialHandle);
                    if (!material)
                    {
                        continue;
                    }

                    if (!material->renderHandle())
                    {
                        auto materialData = renderer::Renderer::MaterialData{};
                        materialData.diffuseColor = material->diffuseColour();
                        if (material->diffuseTexture())
                        {
                            auto image = database.image(*material->diffuseTexture());
                            if (image)
                            {
                                if (!image->renderHandle())
                                {
                                    image->setRenderHandle(
                                        renderer_.addImage(image->width(), image->height(), image->data()));
                                }

                                materialData.diffuseTexture = image->renderHandle();
                            }
                        }

                        material->setRenderHandle(renderer_.addMaterial(materialData));
                    }
                }
            }
        });
}

void RenderSystem::update(Scene& scene, const renderer::Camera& camera)
{
    auto drawInfo = renderer::Renderer::SceneDrawInfo{};

    if (scene.environment.skybox)
    {
        if (auto skybox = scene.assetDatabase.skybox(*scene.environment.skybox))
        {
            drawInfo.skyboxHandle = skybox->renderHandle();
        }
    }

    // Currenty walk the tree twice - need to improve this to walk once
    // Collect lighting
    auto directionalLights = std::vector<DirectionalLight>{};
    scene.entityGraph.visit(
        [&directionalLights](Entity& entity)
        {
            if (!entity.directionalLight)
            {
                return;
            }

            directionalLights.push_back(*entity.directionalLight);
        });

    // Only support one global light for now
    if (!directionalLights.empty())
    {
        drawInfo.globalLightDirection = directionalLights.at(0).direction;
    }

    // Collect render information
    scene.entityGraph.visit(
        [&database = scene.assetDatabase, &drawInfo](Entity& entity)
        {
            if (!entity.prefabHandle)
            {
                return;
            }

            auto prefab = database.prefab(*entity.prefabHandle);
            if (!prefab)
            {
                return;
            }

            for (const auto& instance : prefab->meshInstances)
            {
                if (!instance.materialHandle)
                {
                    continue; // later, use a default material instead of skipping
                }

                auto mesh = database.mesh(instance.meshHandle);
                auto material = database.material(*instance.materialHandle);

                if (!mesh || !material)
                {
                    continue;
                }

                if (!mesh->renderHandle() || !material->renderHandle())
                {
                    continue;
                }

                drawInfo.drawCommands.push_back(
                    renderer::DrawCommand{*mesh->renderHandle(), *material->renderHandle(), entity.transform});
            }
        });

    renderer_.renderScene(camera, drawInfo);
}
} // namespace scene
