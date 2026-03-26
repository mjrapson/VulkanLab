// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "renderer/descriptor_set_allocator.h"
#include "renderer/draw_command.h"
#include "renderer/gpu_device.h"
#include "renderer/instance.h"
#include "renderer/pipeline.h"
#include "renderer/resources.h"

#include <core/vertex.h>

#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace window
{
class Window;
} // namespace window

namespace renderer
{
struct Camera;
class GpuDevice;

class Renderer
{
  public:
    struct MaterialData
    {
        glm::vec3 diffuseColor;
        std::optional<ImageHandle> diffuseTexture;
    };

    struct FaceData
    {
        uint32_t width;
        uint32_t height;
        std::span<const std::byte> data;
    };

    struct SceneDrawInfo
    {
        std::vector<DrawCommand> drawCommands;
        std::optional<renderer::SkyboxHandle> skyboxHandle;
        glm::vec3 globalLightDirection;
    };

    explicit Renderer(const window::Window& window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&& other) = delete;
    Renderer& operator=(Renderer&& other) = delete;

    void renderScene(const Camera& camera, const SceneDrawInfo& info);
    void renderLoadingScreen(LoadingScreenHandle loadingScreenHandle);

    void reset();

    void windowResized(int width, int height);

    LoadingScreenHandle addLoadingScreenImage(uint32_t width, uint32_t height, std::span<const std::byte> data);
    ImageHandle addImage(uint32_t width, uint32_t height, std::span<const std::byte> data);
    MaterialHandle addMaterial(const MaterialData& data);
    MeshHandle addMesh(std::span<const core::Vertex> vertices, std::span<const uint32_t> indices);
    SkyboxHandle addSkybox(const std::array<FaceData, 6>& data);

  private:
    void createSwapchain();
    void createCommandBuffers();
    void createSyncObjects();
    void createSamplers();
    void createCameraBuffers();
    void createDirectionalLightBuffers();
    void createShadowMapDescriptorSets();

    void createShadowPass();
    void createGeometryPass();
    void createSkyboxPass();
    void createLoadingScreenPass();

    void recreateSwapchain();
    void resizeGeometryPass();

    void renderFrame(std::function<void(const vk::raii::CommandBuffer&)> recordCommands);

  private:
    vk::raii::Context context_;
    Instance instance_;
    vk::raii::SurfaceKHR surface_;
    GpuDevice gpuDevice_;

    // Swapchain
    vk::raii::SwapchainKHR swapchain_{nullptr};
    std::vector<vk::Image> swapchainImages_;
    std::vector<vk::raii::ImageView> swapchainImageViews_;
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    vk::Format surfaceFormat_;
    vk::Extent2D swapchainExtent_;

    bool windowMinimized_{false};
    bool swapchainRebuildRequired_{false};

    vk::raii::CommandPool commandPool_{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Fence> drawFences_;
    uint32_t currentFrameIndex_{0};
    uint32_t currentSwapchainImageIndex_{0};

    DescriptorSetAllocator cameraDescriptor_;
    DescriptorSetAllocator materialDescriptor_;
    DescriptorSetAllocator directionalLightDescriptor_;
    DescriptorSetAllocator skyboxDescriptor_;
    DescriptorSetAllocator loadingScreenImageDescriptor_;
    DescriptorSetAllocator shadowMapImageDescriptor_;

    Image emptyImage_;

    std::vector<vk::raii::Buffer> cameraUniformBuffers_;
    std::vector<vk::raii::DeviceMemory> cameraUniformBuffersMemory_;
    std::vector<void*> cameraUniformBuffersMappedMemory_;
    std::vector<vk::raii::DescriptorSet> cameraDescriptorSets_;

    vk::raii::Buffer directionalLightUniformBuffer_{nullptr};
    vk::raii::DeviceMemory directionalLightUniformBufferMemory_{nullptr};
    void* directionalLightUniformBufferMappedMemory_{nullptr};
    vk::raii::DescriptorSet directionalLightDescriptorSet_{nullptr};

    vk::raii::DescriptorSet shadowMapDescriptorSet_{nullptr};

    Resources resources_;
    vk::raii::Sampler imageSampler_{nullptr};
    vk::raii::Sampler shadowSampler_{nullptr};

    vk::raii::Image shadowMapImage_{nullptr};
    vk::raii::DeviceMemory shadowMapImageMemory_{nullptr};
    vk::raii::ImageView shadowMapImageView_{nullptr};

    vk::raii::Image depthTargetImage_{nullptr};
    vk::raii::DeviceMemory depthTargetImageMemory_{nullptr};
    vk::raii::ImageView depthTargetImageView_{nullptr};

    Pipeline shadowPass_;
    Pipeline geometryPass_;
    Pipeline skyboxPass_;
    Pipeline loadingScreenPass_;

    float shadowDistance_{50.0f};
    float lightDistance_{100.0f};
};
} // namespace renderer
