/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "scene/scene_loader.h"

#include "scene/scene.h"

#include <core/file_system.h>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace scene
{
constexpr auto positionKey = "position";
constexpr auto rotationKey = "rotation";
constexpr auto scaleKey = "scale";
constexpr auto transformComponentKey = "transformComponent";
constexpr auto renderComponentKey = "renderComponent";
constexpr auto prefabKey = "prefab";
constexpr auto nameKey = "name";
constexpr auto pathKey = "path";
constexpr auto cameraKey = "camera";
constexpr auto skyboxKey = "skybox";
constexpr auto texturesKey = "textures";
constexpr auto prefabsKey = "prefabs";
constexpr auto entitiesKey = "entities";
constexpr auto skyboxesKey = "skyboxes";

glm::vec3
loadVec3(const nlohmann::json& json, const std::string& param1, const std::string& param2, const std::string& param3)
{
    return glm::vec3{json[param1], json[param2], json[param3]};
}

glm::vec3 loadXYZ(const nlohmann::json& json)
{
    return loadVec3(json, "x", "y", "z");
}

glm::vec3 loadRGB(const nlohmann::json& json)
{
    return loadVec3(json, "r", "g", "b");
}

TransformComponent loadTransformComponent(const nlohmann::json& json)
{
    auto transform = TransformComponent{};

    if (auto itr = json.find(positionKey); itr != json.end())
    {
        transform.position = loadXYZ(*itr);
    }
    if (auto itr = json.find(rotationKey); itr != json.end())
    {
        transform.rotation = loadXYZ(*itr);
    }
    if (auto itr = json.find(scaleKey); itr != json.end())
    {
        transform.scale = loadXYZ(*itr);
    }

    return transform;
}

std::optional<RenderComponent> loadRenderComponent(const nlohmann::json& json)
{
    if (!json.contains(prefabKey))
    {
        return std::nullopt;
    }

    return RenderComponent{.prefabId = json[prefabKey]};
}

void loadEntity(const nlohmann::json& json, Scene& scene)
{
    if (!json.contains(nameKey))
    {
        return;
    }

    auto entity = Entity{};
    entity.name = json[nameKey];

    if (auto itr = json.find(transformComponentKey); itr != json.end())
    {
        entity.transformComponent = loadTransformComponent(*itr);
    }
    if (auto itr = json.find(renderComponentKey); itr != json.end())
    {
        entity.renderComponent = loadRenderComponent(*itr);
    }

    scene.entities.push_back(std::move(entity));
}

void loadPrefab(const nlohmann::json& json, Scene& scene)
{
    if (!json.contains(nameKey) || !json.contains(pathKey))
    {
        return;
    }

    auto prefab = Prefab{};
    prefab.name = json[nameKey];
    prefab.path = json[pathKey];

    scene.prefabs.push_back(std::move(prefab));
}

void loadSkybox(const nlohmann::json& json, Scene& scene)
{
    if (!json.contains(nameKey) || !json.contains(texturesKey))
    {
        return;
    }

    auto skybox = Skybox{};
    skybox.name = json[nameKey];
    skybox.pxPath = json[texturesKey]["px"];
    skybox.pyPath = json[texturesKey]["py"];
    skybox.pzPath = json[texturesKey]["pz"];
    skybox.nxPath = json[texturesKey]["nx"];
    skybox.nyPath = json[texturesKey]["ny"];
    skybox.nzPath = json[texturesKey]["nz"];

    scene.skyboxes.push_back(std::move(skybox));
}

void loadCamera(const nlohmann::json& json, Scene& scene)
{
    if (json.contains(skyboxKey))
    {
        scene.camera.skybox = json[skyboxKey];
    }
}

std::unique_ptr<Scene> loadScene(const std::filesystem::path& path)
{
    auto filestream = std::ifstream{path};

    auto sceneJson = nlohmann::json::parse(filestream);

    auto scene = std::make_unique<Scene>();

    for (const auto& prefabJson : sceneJson[prefabsKey])
    {
        loadPrefab(prefabJson, *scene);
    }

    for (const auto& skyboxJson : sceneJson[skyboxesKey])
    {
        loadSkybox(skyboxJson, *scene);
    }

    for (const auto& entityJson : sceneJson[entitiesKey])
    {
        loadEntity(entityJson, *scene);
    }

    if(sceneJson.contains(cameraKey))
    {
        loadCamera(sceneJson[cameraKey], *scene);
    }

    return scene;
}
} // namespace scene
