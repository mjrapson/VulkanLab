// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/buffers.h"
#include "renderer/data.h"
#include "renderer/draw_command.h"
#include "renderer/gpu_device.h"
#include "renderer/gpu_objects.h"
#include "renderer/handles.h"
#include "renderer/instance.h"
#include "renderer/resources.h"
#include "renderer/swapchain.h"

#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <vector>

namespace window
{
class Window;
} // namespace window

namespace renderer
{
struct AssetData;
struct Camera;
class GeometryPass;
class GpuDevice;
struct ImageData;
class LoadingScreenPass;
class ShadowMapPass;
class SkyboxPass;

class Renderer
{
  public:
    explicit Renderer(const window::Window& window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&& other) = delete;
    Renderer& operator=(Renderer&& other) = delete;

    void setLoadingScreenImageData(const ImageDataContainer& imageData);

    struct SceneDrawInfo
    {
        std::vector<DrawCommand> drawCommands;
        std::optional<renderer::SkyboxHandle> skyboxHandle;
        glm::vec3 globalLightDirection;
    };

    void renderScene(const Camera& camera, const SceneDrawInfo& info);

    void renderLoadingScreen(ImageHandle loadingScreenHandle);

    void windowResized(int width, int height);

    void setData(const AssetData& data);

  private:
    void createCommandBuffers();
    void createSyncObjects();
    void createCameraBuffers();
    void createDirectionalLightBuffers();
    void createRenderPasses();

    void renderFrame(std::function<void(const vk::raii::CommandBuffer&)> recordCommands);

    ImageContainer uploadImages(const ImageDataContainer& data);
    void uploadMeshes(const MeshDataContainer& data);
    void uploadMaterials(const MaterialDataContainer& data);
    void uploadSkyboxes(const SkyboxDataContainer& data);

  private:
    vk::raii::Context context_;
    Instance instance_;
    vk::raii::SurfaceKHR surface_;
    GpuDevice gpuDevice_;
    Swapchain swapchain_;

    vk::Extent2D windowExtent_;
    bool windowMinimized_{false};

    vk::raii::CommandPool commandPool_{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Fence> drawFences_;
    uint32_t currentFrameIndex_{0};

    Buffers buffers_;
    Resources resources_;

    std::unique_ptr<LoadingScreenPass> loadingScreenPass_{nullptr};
    std::unique_ptr<SkyboxPass> skyboxPass_{nullptr};
    std::unique_ptr<GeometryPass> geometryPass_{nullptr};
    std::unique_ptr<ShadowMapPass> shadowMapPass_{nullptr};
};
} // namespace renderer
