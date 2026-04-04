/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "scene/scene_loader.h"

#include "scene/scene_gltf_loader.h"

#include <assets/loaders/image_loader.h>

#include <spdlog/spdlog.h>

namespace scene
{
std::unique_ptr<Scene> loadSceneFromFolder(const std::filesystem::path& path)
{
    // Check folder exists
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
    {
        assert(false && "Scene folder does not exist");
        return nullptr;
    }

    // Check scene.glb exists
    const auto sceneFile = path / "scene.glb";
    if (!std::filesystem::exists(sceneFile))
    {
        assert(false && "scene.glb not found in folder");
        return nullptr;
    }

    auto scene = std::make_unique<Scene>();

    try
    {
        loadGltfScene(sceneFile, *scene);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to load glTF scene: {}", e.what());
        return nullptr;
    }

    // If skybox file exists, also load that (optional)
    const auto skyboxFile = path / "skybox.hdr";
    if (std::filesystem::exists(skyboxFile))
    {
        spdlog::debug("Loading skybox {}", skyboxFile.string());
        try
        {
            scene->environment.skybox = scene->assetDatabase.createSkybox(
                assets::createSkyboxImageFromPath(skyboxFile));
        }
        catch (const std::exception& e)
        {
            spdlog::error("skybox.hdr file exists, but failed to load: {}", e.what());
        }
    }

    return scene;
}
} // namespace scene
