// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "vulkan_application.h"

#include <core/file_system.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <fstream>
#include <ranges>
#include <stdexcept>
#include <vector>

constexpr bool validationLayersEnabled()
{
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

bool isDiscreteGpu(const vk::raii::PhysicalDevice& device)
{
    return device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
}

bool supportsGraphicsQueue(const vk::QueueFamilyProperties& properties)
{
    return (properties.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
}

bool supportsSurfacePresentation(uint32_t index, const vk::raii::PhysicalDevice& device,
                                 const vk::raii::SurfaceKHR& surface)
{
    return (device.getSurfaceSupportKHR(index, surface) == VK_TRUE);
}

uint32_t getGraphicsQueueFamilyIndex(const vk::raii::PhysicalDevice& device)
{
    const auto& queueFamilyProperties = device.getQueueFamilyProperties();

    const auto itr = std::ranges::find_if(queueFamilyProperties, supportsGraphicsQueue);

    if (itr == queueFamilyProperties.end())
    {
        throw std::runtime_error("Device does not support graphics queue family");
    }

    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), itr));
}

uint32_t getSurfacePresentationQueueFamilyIndex(const vk::raii::PhysicalDevice& device,
                                                const vk::raii::SurfaceKHR& surface)
{
    const auto& queueFamilyProperties = device.getQueueFamilyProperties();

    const auto itr = std::ranges::find_if(queueFamilyProperties,
                                          [&device, &surface, idx = size_t{0}](const auto&) mutable
                                          {
                                              const auto validQueueFamilyProperty =
                                                  supportsSurfacePresentation(idx, device, surface);

                                              ++idx;
                                              return validQueueFamilyProperty;
                                          });

    if (itr == queueFamilyProperties.end())
    {
        throw std::runtime_error("Device does not support surface presentation queue family");
    }

    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), itr));
}

uint32_t getSurfaceMinImageCount(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities)
{
    const auto desiredImageCount = surfaceCapabilities.minImageCount + 1;

    if (surfaceCapabilities.maxImageCount == 0) // no maximum
    {
        return desiredImageCount;
    }

    return std::min(desiredImageCount, surfaceCapabilities.maxImageCount);
}

bool isPreferredSurfaceFormat(const vk::SurfaceFormatKHR& format)
{
    return format.format == vk::Format::eB8G8R8A8Srgb &&
           format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
}

vk::SurfaceFormatKHR getSurfaceFormat(const vk::PhysicalDevice& device,
                                      const vk::SurfaceKHR& surface)
{
    const auto formats = device.getSurfaceFormatsKHR(surface);

    if (formats.empty())
    {
        throw std::runtime_error("No available surface formats");
    }

    if (auto itr = std::ranges::find_if(formats, isPreferredSurfaceFormat); itr != formats.end())
    {
        return *itr;
    }

    return formats.at(0);
}

vk::Extent2D getSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
    if (capabilities.currentExtent.width != 0xFFFFFFFF)
    {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height)};
}

std::vector<char> readFile(const std::string& filename)
{
    auto file = std::ifstream(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    return buffer;
}

[[nodiscard]] vk::raii::ShaderModule createShaderModule(const vk::raii::Device& device,
                                                        const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                          .pCode = reinterpret_cast<const uint32_t*>(code.data())};

    vk::raii::ShaderModule shaderModule{device, createInfo};
    return shaderModule;
}

void transitionImageLayout(const vk::Image& image, const vk::raii::CommandBuffer& commandBuffer,
                           vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                           vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
                           vk::PipelineStageFlags2 srcStageMask,
                           vk::PipelineStageFlags2 dstStageMask)
{
    const auto barrier =
        vk::ImageMemoryBarrier2{.srcStageMask = srcStageMask,
                                .srcAccessMask = srcAccessMask,
                                .dstStageMask = dstStageMask,
                                .dstAccessMask = dstAccessMask,
                                .oldLayout = oldLayout,
                                .newLayout = newLayout,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image = image,
                                .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                     .baseMipLevel = 0,
                                                     .levelCount = 1,
                                                     .baseArrayLayer = 0,
                                                     .layerCount = 1}};
    const auto dependencyInfo = vk::DependencyInfo{
        .dependencyFlags = {}, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};

    commandBuffer.pipelineBarrier2(dependencyInfo);
}

const auto validationLayers = std::vector<char const*>{"VK_LAYER_KHRONOS_validation"};
const auto deviceExtensions = std::vector<const char*>{
    vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName, vk::KHRCreateRenderpass2ExtensionName};

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
{
    if (severity < vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
    {
        return VK_FALSE;
    }

    spdlog::error("{} {}", to_string(type), pCallbackData->pMessage);

    return VK_TRUE;
}

VulkanApplication::VulkanApplication() {}

VulkanApplication::~VulkanApplication()
{
    if (window_)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        spdlog::info("GLFW Window destroyed");
    }

    if (glfwInitialised_)
    {
        glfwTerminate();
        spdlog::info("GLFW terminated");
    }
}

void VulkanApplication::init(int windowWidth, int windowHeight, const std::string& windowTitle)
{
    spdlog::info("Initializing GLFW");
    initGlfw();

    spdlog::info("Initializing GLFW window");
    initWindow(windowWidth, windowHeight, windowTitle);

    spdlog::info("Initializing Vulkan");
    initVulkan();
}

void VulkanApplication::run()
{
    spdlog::info("Running");

    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();

        drawFrame();
    }

    device_.waitIdle();
}

void VulkanApplication::initGlfw()
{
    glfwSetErrorCallback([](int errorCode, const char* description)
                         { spdlog::error("GLFW error {}: {}", errorCode, description); });

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwInitialised_ = true;
}

void VulkanApplication::initWindow(int windowWidth, int windowHeight,
                                   const std::string& windowTitle)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(windowWidth, windowHeight, windowTitle.c_str(), nullptr, nullptr);
    if (!window_)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }
}

void VulkanApplication::initVulkan()
{
    auto version = uint32_t{0};
    auto pfnEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));

    if (pfnEnumerateInstanceVersion)
    {
        pfnEnumerateInstanceVersion(&version);
    }
    else
    {
        version = VK_API_VERSION_1_0;
    }

    const auto major = VK_VERSION_MAJOR(version);
    const auto minor = VK_VERSION_MINOR(version);
    const auto patch = VK_VERSION_PATCH(version);

    spdlog::info("Vulkan API version: {}.{}.{}", major, minor, patch);

    spdlog::info("Creating Vulkan instance");
    createVulkanInstance();

    if (validationLayersEnabled())
    {
        spdlog::info("Setting up Vulkan debug messaging");
        setupDebugMessenger();
    }

    spdlog::info("Creating window surface");
    createSurface();

    spdlog::info("Finding physical GPU device");
    pickPhysicalDevice();

    spdlog::info("Creating logical GPU device");
    createLogicalDevice();

    spdlog::info("Creating swapchain");
    createSwapchain();

    spdlog::info("Creating swapchain image views");
    createImageViews();

    spdlog::info("Creating graphics pipeline");
    createGraphicsPipeline();

    spdlog::info("Creating command pool");
    createCommandPool();

    spdlog::info("Creating command buffer");
    createCommandBuffer();

    spdlog::info("Creating sync objects");
    createSyncObjects();
}

void VulkanApplication::createVulkanInstance()
{
    constexpr auto appInfo = vk::ApplicationInfo{.pApplicationName = "Vulkan Demo",
                                                 .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                                 .pEngineName = "Vulkan Demo Engine",
                                                 .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                                 .apiVersion = vk::ApiVersion14};

    const auto requiredExtensions = getRequiredExtensions();
    const auto requiredLayers = getRequiredLayers();

    auto createInfo = vk::InstanceCreateInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()};

    instance_ = vk::raii::Instance(context_, createInfo);
}

void VulkanApplication::setupDebugMessenger()
{
    const auto severityFlags = (vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    const auto messageTypeFlags = (vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    const auto debugUtilsMessengerCreateInfoEXT =
        vk::DebugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
                                             .messageType = messageTypeFlags,
                                             .pfnUserCallback = &debugCallback};

    debugMessenger_ = instance_.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

void VulkanApplication::createSurface()
{
    auto surface = VkSurfaceKHR{VK_NULL_HANDLE};
    if (glfwCreateWindowSurface(*instance_, window_, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface");
    }

    surface_ = vk::raii::SurfaceKHR(instance_, surface);
}

void VulkanApplication::pickPhysicalDevice()
{
    const auto devices = instance_.enumeratePhysicalDevices();
    if (devices.empty())
    {
        throw std::runtime_error("Failed to find GPU with Vulkan support");
    }

    auto suitableDevices = std::vector<vk::raii::PhysicalDevice>{};
    for (const auto& device : devices)
    {
        if (isDeviceSuitable(device))
        {
            suitableDevices.push_back(device);
        }
    }

    if (suitableDevices.empty())
    {
        throw std::runtime_error("Failed to find a GPU with suitable Vulkan support");
    }

    physicalDevice_ = selecteBestDevice(suitableDevices);

    spdlog::info("Selected GPU: {}", std::string{physicalDevice_.getProperties().deviceName});
}

void VulkanApplication::createLogicalDevice()
{
    graphicsQueueFamilyIndex_ = getGraphicsQueueFamilyIndex(physicalDevice_);

    const auto surfacePresentationQueueFamilyIndex =
        getSurfacePresentationQueueFamilyIndex(physicalDevice_, surface_);

    auto queuePriority = 0.5f;
    const auto deviceQueueCreateInfo =
        vk::DeviceQueueCreateInfo{.queueFamilyIndex = graphicsQueueFamilyIndex_,
                                  .queueCount = 1,
                                  .pQueuePriorities = &queuePriority};

    const auto featureChain =
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>{
            {},
            {.shaderDrawParameters = true},
            {.synchronization2 = true, .dynamicRendering = true},
            {.extendedDynamicState = true}};

    const auto deviceCreateInfo = vk::DeviceCreateInfo{
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()};

    device_ = vk::raii::Device(physicalDevice_, deviceCreateInfo);
    graphicsQueue_ = vk::raii::Queue(device_, graphicsQueueFamilyIndex_, 0);
    presentQueue_ = vk::raii::Queue(device_, surfacePresentationQueueFamilyIndex, 0);
}

void VulkanApplication::createSwapchain()
{
    const auto surfaceCapabilities = physicalDevice_.getSurfaceCapabilitiesKHR(*surface_);
    swapchainExtent_ = getSwapchainExtent(surfaceCapabilities, window_);
    surfaceFormat_ = getSurfaceFormat(physicalDevice_, surface_);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .surface = *surface_,
        .minImageCount = getSurfaceMinImageCount(surfaceCapabilities),
        .imageFormat = surfaceFormat_.format,
        .imageColorSpace = surfaceFormat_.colorSpace,
        .imageExtent = swapchainExtent_,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = true};

    swapchain_ = vk::raii::SwapchainKHR(device_, swapChainCreateInfo);
    swapchainImages_ = swapchain_.getImages();
}

void VulkanApplication::createImageViews()
{
    swapchainImageViews_.clear();

    auto imageViewCreateInfo =
        vk::ImageViewCreateInfo{.viewType = vk::ImageViewType::e2D,
                                .format = surfaceFormat_.format,
                                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

    for (const auto& image : swapchainImages_)
    {
        imageViewCreateInfo.image = image;
        swapchainImageViews_.emplace_back(device_, imageViewCreateInfo);
    }
}

void VulkanApplication::createGraphicsPipeline()
{
    // Shader-progammable stages
    auto vertexShaderModule =
        createShaderModule(device_, readFile(GetShaderDir() / "basic.vert.spv"));

    auto fragmentShaderModule =
        createShaderModule(device_, readFile(GetShaderDir() / "basic.frag.spv"));

    const auto vertShaderStageInfo =
        vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eVertex,
                                          .module = vertexShaderModule,
                                          .pName = "vertMain"};

    const auto fragShaderStageInfo =
        vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eFragment,
                                          .module = fragmentShaderModule,
                                          .pName = "fragMain"};

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Fixed function stages
    const auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};

    const auto inputAssembly =
        vk::PipelineInputAssemblyStateCreateInfo{.topology = vk::PrimitiveTopology::eTriangleList};

    const auto dynamicStates =
        std::vector<vk::DynamicState>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    const auto dynamicState = vk::PipelineDynamicStateCreateInfo{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    const auto viewportState =
        vk::PipelineViewportStateCreateInfo{.viewportCount = 1, .scissorCount = 1};

    const auto rasterizer =
        vk::PipelineRasterizationStateCreateInfo{.depthClampEnable = vk::False,
                                                 .rasterizerDiscardEnable = vk::False,
                                                 .polygonMode = vk::PolygonMode::eFill,
                                                 .cullMode = vk::CullModeFlagBits::eBack,
                                                 .frontFace = vk::FrontFace::eClockwise,
                                                 .depthBiasEnable = vk::False,
                                                 .depthBiasSlopeFactor = 1.0f,
                                                 .lineWidth = 1.0f};

    const auto multisampling = vk::PipelineMultisampleStateCreateInfo{
        .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

    const auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{
        .blendEnable = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

    const auto colorBlending =
        vk::PipelineColorBlendStateCreateInfo{.logicOpEnable = vk::False,
                                              .logicOp = vk::LogicOp::eCopy,
                                              .attachmentCount = 1,
                                              .pAttachments = &colorBlendAttachment};

    const auto pipelineLayoutInfo =
        vk::PipelineLayoutCreateInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};

    pipelineLayout_ = vk::raii::PipelineLayout(device_, pipelineLayoutInfo);

    // Render passes (dynamic rendering)
    const auto pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{
        .colorAttachmentCount = 1, .pColorAttachmentFormats = &surfaceFormat_.format};

    const auto pipelineInfo = vk::GraphicsPipelineCreateInfo{.pNext = &pipelineRenderingCreateInfo,
                                                             .stageCount = 2,
                                                             .pStages = shaderStages,
                                                             .pVertexInputState = &vertexInputInfo,
                                                             .pInputAssemblyState = &inputAssembly,
                                                             .pViewportState = &viewportState,
                                                             .pRasterizationState = &rasterizer,
                                                             .pMultisampleState = &multisampling,
                                                             .pColorBlendState = &colorBlending,
                                                             .pDynamicState = &dynamicState,
                                                             .layout = pipelineLayout_,
                                                             .renderPass = nullptr};

    graphicsPipeline_ = vk::raii::Pipeline(device_, nullptr, pipelineInfo);
}

void VulkanApplication::createCommandPool()
{
    const auto poolInfo =
        vk::CommandPoolCreateInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                  .queueFamilyIndex = graphicsQueueFamilyIndex_};

    commandPool_ = vk::raii::CommandPool(device_, poolInfo);
}

void VulkanApplication::createCommandBuffer()
{
    const auto allocInfo = vk::CommandBufferAllocateInfo{.commandPool = commandPool_,
                                                         .level = vk::CommandBufferLevel::ePrimary,
                                                         .commandBufferCount = 1};

    commandBuffer_ = std::move(vk::raii::CommandBuffers(device_, allocInfo).front());
}

void VulkanApplication::createSyncObjects()
{
    presentCompleteSemaphore_ = vk::raii::Semaphore(device_, vk::SemaphoreCreateInfo());
    renderFinishedSemaphore_ = vk::raii::Semaphore(device_, vk::SemaphoreCreateInfo());
    drawFence_ = vk::raii::Fence(device_, {.flags = vk::FenceCreateFlagBits::eSignaled});
}

void VulkanApplication::drawFrame()
{
    graphicsQueue_.waitIdle();

    auto [result, imageIndex] =
        swapchain_.acquireNextImage(UINT64_MAX, *presentCompleteSemaphore_, nullptr);

    recordCommands(imageIndex);

    device_.resetFences(*drawFence_);

    const auto waitDestinationStageMask =
        vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);

    const auto submitInfo = vk::SubmitInfo{.waitSemaphoreCount = 1,
                                           .pWaitSemaphores = &*presentCompleteSemaphore_,
                                           .pWaitDstStageMask = &waitDestinationStageMask,
                                           .commandBufferCount = 1,
                                           .pCommandBuffers = &*commandBuffer_,
                                           .signalSemaphoreCount = 1,
                                           .pSignalSemaphores = &*renderFinishedSemaphore_};

    graphicsQueue_.submit(submitInfo, *drawFence_);

    while (vk::Result::eTimeout == device_.waitForFences(*drawFence_, vk::True, UINT64_MAX))
        ;

    const auto presentInfoKHR = vk::PresentInfoKHR{.waitSemaphoreCount = 1,
                                                   .pWaitSemaphores = &*renderFinishedSemaphore_,
                                                   .swapchainCount = 1,
                                                   .pSwapchains = &*swapchain_,
                                                   .pImageIndices = &imageIndex};

    result = presentQueue_.presentKHR(presentInfoKHR);
    switch (result)
    {
        case vk::Result::eSuccess:
            break;
        case vk::Result::eSuboptimalKHR:
            // std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
            break;
        default:
            break; // an unexpected result is returned!
    }
}

void VulkanApplication::recordCommands(uint32_t imageIndex)
{
    commandBuffer_.begin({});

    transitionImageLayout(swapchainImages_.at(imageIndex), commandBuffer_,
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                          {}, // srcAccessMask (no need to wait for previous operations)
                          vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput  // dstStage
    );

    const auto clearColor = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
    const auto attachmentInfo =
        vk::RenderingAttachmentInfo{.imageView = swapchainImageViews_.at(imageIndex),
                                    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                    .loadOp = vk::AttachmentLoadOp::eClear,
                                    .storeOp = vk::AttachmentStoreOp::eStore,
                                    .clearValue = clearColor};

    const auto renderingInfo =
        vk::RenderingInfo{.renderArea = {.offset = {0, 0}, .extent = swapchainExtent_},
                          .layerCount = 1,
                          .colorAttachmentCount = 1,
                          .pColorAttachments = &attachmentInfo};

    commandBuffer_.beginRendering(renderingInfo);
    commandBuffer_.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline_);

    commandBuffer_.setViewport(
        0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapchainExtent_.width),
                        static_cast<float>(swapchainExtent_.height), 0.0f, 1.0f));
    commandBuffer_.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent_));

    commandBuffer_.draw(3, 1, 0, 0);

    commandBuffer_.endRendering();

    transitionImageLayout(swapchainImages_.at(imageIndex), commandBuffer_,
                          vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                          vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                          {},                                                 // dstAccessMask
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                          vk::PipelineStageFlagBits2::eBottomOfPipe           // dstStage
    );

    commandBuffer_.end();
}

std::vector<char const*> VulkanApplication::getRequiredExtensions() const
{
    auto glfwExtensionCount = uint32_t{0};
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto extensions = std::vector<const char*>{glfwExtensions, glfwExtensions + glfwExtensionCount};
    if (validationLayersEnabled())
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    auto extensionProperties = context_.enumerateInstanceExtensionProperties();
    for (size_t i = 0; i < extensions.size(); ++i)
    {
        if (std::ranges::none_of(
                extensionProperties, [extension = extensions[i]](auto const& extensionProperty)
                { return strcmp(extensionProperty.extensionName, extension) == 0; }))
        {
            throw std::runtime_error("Required extension not supported: " +
                                     std::string(extensions[i]));
        }
    }

    return extensions;
}

std::vector<char const*> VulkanApplication::getRequiredLayers() const
{
    if (!validationLayersEnabled())
    {
        return std::vector<char const*>{};
    }

    const auto layerProperties = context_.enumerateInstanceLayerProperties();
    for (const auto& requiredLayer : validationLayers)
    {
        if (std::ranges::none_of(layerProperties, [requiredLayer](auto const& layerProperty)
                                 { return strcmp(layerProperty.layerName, requiredLayer) == 0; }))
        {
            throw std::runtime_error("One or more required layers are not supported");
        }
    }

    return validationLayers;
}

bool VulkanApplication::isDeviceSuitable(vk::raii::PhysicalDevice device) const
{
    const auto properties = device.getProperties();
    const auto deviceName = std::string{properties.deviceName};

    if (properties.apiVersion < VK_API_VERSION_1_3)
    {
        spdlog::info("Skipping {} - Vulkan API version too low ({})", deviceName,
                     properties.apiVersion);
        return false;
    }

    if (std::ranges::none_of(device.getQueueFamilyProperties(), supportsGraphicsQueue))
    {
        spdlog::info("Skipping {} - Does not support graphics queue family", deviceName);
        return false;
    }

    const auto extensionProperties = device.enumerateDeviceExtensionProperties();
    bool hasAllRequiredExtensions = true;
    for (const auto& requiredExtension : deviceExtensions)
    {
        if (std::ranges::none_of(
                extensionProperties, [&requiredExtension](auto const& extension)
                { return strcmp(extension.extensionName, requiredExtension) == 0; }))
        {
            hasAllRequiredExtensions = false;
            spdlog::info("Skipping {} - Does not support required device extension: {}", deviceName,
                         requiredExtension);
        }
    }

    if (!hasAllRequiredExtensions)
    {
        return false;
    }

    return true;
}

vk::raii::PhysicalDevice
VulkanApplication::selecteBestDevice(const std::vector<vk::raii::PhysicalDevice>& devices) const
{
    if (devices.empty())
    {
        throw std::invalid_argument("No devices to select between!");
    }

    if (devices.size() == 1)
    {
        return devices.at(0);
    }

    const auto itr = std::find_if(devices.begin(), devices.end(), isDiscreteGpu);
    if (itr != devices.end())
    {
        return *itr;
    }

    return devices.at(0);
}
