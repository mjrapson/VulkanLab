// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/buffer_object.h"
#include "renderer/data.h"
#include "renderer/draw_command.h"
#include "renderer/gpu_device.h"
#include "renderer/gpu_objects.h"
#include "renderer/handles.h"
#include "renderer/instance.h"
#include "renderer/swapchain.h"

#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace window
{
class Window;
} // namespace window

namespace renderer
{
struct AssetData;
class GeometryPass;
class GpuDevice;
struct ImageData;
class LoadingScreenPass;
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

    void setLoadingScreenImage(const ImageData& imageData);

    struct SceneDrawInfo
    {
        std::vector<DrawCommand> drawCommands;
        std::optional<renderer::SkyboxHandle> skyboxHandle;
        glm::mat4 cameraProjection;
        glm::mat4 cameraView;
        glm::vec3 globalLightDirection;
    };

    void renderScene(const SceneDrawInfo& info);

    void renderLoadingScreen();

    void windowResized(int width, int height);

    void setData(const AssetData& data);

  private:
    void createSwapchain();
    void createCommandBuffers();
    void createSyncObjects();
    void createCameraBuffers();
    void createRenderPasses();

    void recreateSwapchain();

    void renderFrame(std::function<void(uint32_t, const vk::raii::CommandBuffer&)> recordCommands);

    void uploadImages(const ImageDataContainer& data);
    void uploadMeshes(const MeshDataContainer& data);
    void uploadMaterials(const MaterialDataContainer& data);
    void uploadSkyboxes(const SkyboxDataContainer& data);

  private:
    vk::raii::Context context_;
    Instance instance_;
    vk::raii::SurfaceKHR surface_;
    GpuDevice gpuDevice_;
    Swapchain swapchain_;

    int windowWidth_{0};
    int windowHeight_{0};
    bool windowResized_{false};
    bool windowMinimized_{false};

    vk::raii::CommandPool commandPool_{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence> drawFences_;
    uint32_t currentFrameIndex_{0};

    Image emptyImage_;
    BufferObject meshVertexBuffer_;
    BufferObject meshIndexBuffer_;
    BufferObject materialUbo_;
    std::vector<BufferObject> cameraUbos_;

    std::unordered_map<MeshHandle, Mesh, core::Hash<MeshHandle>> meshGpuData_;
    std::unordered_map<MaterialHandle, Material, core::Hash<MaterialHandle>> materialGpuData_;
    std::unordered_map<ImageHandle, Image, core::Hash<ImageHandle>> imageGpuData_;
    std::unordered_map<SkyboxHandle, Skybox, core::Hash<SkyboxHandle>> skyboxGpuData_;

    std::unique_ptr<LoadingScreenPass> loadingScreenPass_{nullptr};
    std::unique_ptr<SkyboxPass> skyboxPass_{nullptr};
    std::unique_ptr<GeometryPass> geometryPass_{nullptr};
};
} // namespace renderer
