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

namespace assets
{
class Image;
}

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

    void setLoadingScreenImage(const assets::Image& image);

    void renderScene(std::span<const DrawCommand> drawCommands,
                     std::optional<assets::SkyboxHandle> skyboxHandle,
                     const Camera& camera);
    void renderLoadingScreen();

    void windowResized(int width, int height);

    void setResources(std::unique_ptr<GpuResourceCache> gpuResources);

    const GpuDevice& device() const;

  private:
    void createSwapchain();
    void createCommandBuffers();
    void createSyncObjects();
    void createCameraBuffers();
    void createRenderPasses();

    void recreateSwapchain();

    void renderFrame(std::function<void(uint32_t, const vk::raii::CommandBuffer&)> recordCommands);

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

    std::vector<vk::raii::Buffer> cameraUboBuffers_;
    std::vector<vk::raii::DeviceMemory> cameraUboBuffersMemory_;
    std::vector<void*> cameraUboMappedMemory_;

    std::unique_ptr<GpuResourceCache> gpuResources_{nullptr};

    std::unique_ptr<LoadingScreenPass> loadingScreenPass_{nullptr};
    std::unique_ptr<SkyboxPass> skyboxPass_{nullptr};
    std::unique_ptr<GeometryPass> geometryPass_{nullptr};
};
} // namespace renderer
