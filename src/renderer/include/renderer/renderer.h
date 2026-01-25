// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/draw_command.h"

#include <assets/material.h>

#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <vector>

namespace assets
{
class AssetDatabase;
}

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
    Renderer(const vk::raii::Instance& instance,
             const vk::raii::SurfaceKHR& surface,
             const GpuDevice& gpuDevice,
             int windowWidth,
             int windowHeight);

    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&& other) = delete;
    Renderer& operator=(Renderer&& other) = delete;

    void renderFrame(const renderer::Camera& camera, const std::vector<DrawCommand>& drawCommands);
    void windowResized(int width, int height);

    void setResources(const assets::AssetDatabase& db);

  private:
    void createSwapchain();
    void createSwapchainImageViews();

    void createCameraDescriptorPool();
    void createMaterialDescriptorPools(uint32_t materialCount);
    void createDescriptorSetLayouts();
    void createCommandBuffers();
    void createSyncObjects();
    void createCameraBuffers();

    void createDescriptorSets();

    void recreateSwapchain();
    void recordCommands(uint32_t imageIndex,
                        const vk::raii::CommandBuffer& commandBuffer,
                        const renderer::Camera& camera,
                        const std::vector<DrawCommand>& drawCommands);

    void createDepthBufferImage();
    void createRenderPasses();

  private:
    const vk::raii::Instance& instance_;
    const vk::raii::SurfaceKHR& surface_;
    const GpuDevice& gpuDevice_;

    int windowWidth_{0};
    int windowHeight_{0};
    bool windowResized_{false};
    bool windowMinimized_{false};

    vk::raii::SwapchainKHR swapchain_{nullptr};
    vk::Extent2D swapchainExtent_;
    vk::SurfaceFormatKHR surfaceFormat_;
    std::vector<vk::Image> swapchainImages_;
    std::vector<vk::raii::ImageView> swapchainImageViews_;
    vk::raii::DescriptorPool cameraDescriptorPool_{nullptr};
    vk::raii::DescriptorSetLayout cameraDescriptorSetLayout_{nullptr};
    vk::raii::DescriptorSetLayout materialDescriptorSetLayout_{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence> drawFences_;
    uint32_t currentFrameIndex_{0};

    vk::raii::Image depthImage_{nullptr};
    vk::raii::DeviceMemory depthImageMemory_{nullptr};
    vk::raii::ImageView depthImageView_{nullptr};

    std::vector<vk::raii::Buffer> cameraUboBuffers_;
    std::vector<vk::raii::DeviceMemory> cameraUboBuffersMemory_;
    std::vector<void*> cameraUboMappedMemory_;
    std::vector<vk::raii::DescriptorSet> cameraDescriptorSets_;

    std::unique_ptr<GpuResourceCache> gpuResources_{nullptr};

    std::unique_ptr<SkyboxPass> skyboxPass_{nullptr};
    std::unique_ptr<GeometryPass> geometryPass_{nullptr};
};
} // namespace renderer
