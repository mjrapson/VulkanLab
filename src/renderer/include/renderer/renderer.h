// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/draw_command.h"
#include "renderer/gpu_device.h"
#include "renderer/instance.h"
#include "renderer/swapchain.h"

#include <assets/handles.h>

#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <vector>

namespace window
{
class Window;
} // namespace window

namespace renderer
{
class Camera;
class GeometryPass;
class GpuDevice;
class GpuResourceCache;
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

    void queueMeshDraw(assets::SubMeshHandle subMeshHandle,
                       assets::MaterialHandle materialHandle,
                       const glm::mat4& transform);

    void renderFrame(const renderer::Camera& camera, const std::optional<assets::SkyboxHandle>& skyboxHandle);

    void windowResized(int width, int height);

    void setResources(std::unique_ptr<GpuResourceCache> gpuResources);

    const GpuDevice& device() const;

  private:
    void createSwapchain();
    void createCommandBuffers();
    void createSyncObjects();
    void createCameraBuffers();

    void recreateSwapchain();
    void recordCommands(uint32_t imageIndex,
                        const vk::raii::CommandBuffer& commandBuffer,
                        const renderer::Camera& camera,
                        const std::optional<assets::SkyboxHandle>& skyboxHandle);
    void createRenderPasses();

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

    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence> drawFences_;
    uint32_t currentFrameIndex_{0};

    std::vector<vk::raii::Buffer> cameraUboBuffers_;
    std::vector<vk::raii::DeviceMemory> cameraUboBuffersMemory_;
    std::vector<void*> cameraUboMappedMemory_;

    std::unique_ptr<GpuResourceCache> gpuResources_{nullptr};

    std::vector<DrawCommand> drawCommands_;

    std::unique_ptr<SkyboxPass> skyboxPass_{nullptr};
    std::unique_ptr<GeometryPass> geometryPass_{nullptr};
};
} // namespace renderer
