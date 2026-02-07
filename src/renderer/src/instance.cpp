/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/instance.h"

#include <spdlog/spdlog.h>

namespace renderer
{
constexpr bool validationLayersEnabled()
{
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

bool validateExtensions(std::span<const char* const> requiredExtensions, const vk::raii::Context& context)
{
    bool allExtensionsValid = true;

    const auto availableExtensions = context.enumerateInstanceExtensionProperties();
    for (const auto requiredExtension : requiredExtensions)
    {
        if (std::ranges::none_of(availableExtensions,
                                 [requiredExtension](const auto& extensionProperty)
                                 {
                                     return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                                 }))
        {
            allExtensionsValid = false;
            spdlog::error("Required extension {} not supported", requiredExtension);
        }
    }

    return allExtensionsValid;
}

bool validateLayers(std::span<const char* const> requiredLayers, const vk::raii::Context& context)
{
    bool allLayersValid = true;

    const auto availableLayers = context.enumerateInstanceLayerProperties();
    for (const auto& requiredLayer : requiredLayers)
    {
        if (std::ranges::none_of(availableLayers,
                                 [requiredLayer](const auto& layerProperty)
                                 {
                                     return strcmp(layerProperty.layerName, requiredLayer) == 0;
                                 }))
        {
            allLayersValid = false;
            spdlog::error("Required validation layer {} not available", requiredLayer);
        }
    }

    return allLayersValid;
}

static vk::Bool32 debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                vk::DebugUtilsMessageTypeFlagsEXT,
                                const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                void*)
{
    if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
    {
        spdlog::error("{}", pCallbackData->pMessage);
    }
    else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
    {
        spdlog::warn("{}", pCallbackData->pMessage);
    }
    else
    {
        spdlog::info("{}", pCallbackData->pMessage);
    }

    return VK_TRUE;
}

Instance::Instance(const vk::raii::Context& context, std::span<const char* const> windowExtensions)
{
    const auto version = vk::enumerateInstanceVersion();
    spdlog::info("Vulkan API version: {}.{}.{}",
                 VK_VERSION_MAJOR(version),
                 VK_VERSION_MINOR(version),
                 VK_VERSION_PATCH(version));

    spdlog::info("Creating Vulkan instance");
    createInstance(context, windowExtensions);

    if (validationLayersEnabled())
    {
        spdlog::info("Setting up Vulkan debug messaging");
        createDebugMessenger();
    }
}

const vk::raii::Instance& Instance::instance() const
{
    return instance_;
}

void Instance::createInstance(const vk::raii::Context& context, std::span<const char* const> windowExtensions)
{
    auto extensions = std::vector<const char*>{windowExtensions.begin(), windowExtensions.end()};
    if (validationLayersEnabled())
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    const auto validationLayers = std::vector<char const*>{"VK_LAYER_KHRONOS_validation"};

    auto appInfo = vk::ApplicationInfo{};
    appInfo.pApplicationName = "Vulkan Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Vulkan Demo Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = vk::ApiVersion14;

    if (!validateExtensions(extensions, context))
    {
        throw std::runtime_error("Requested extensions not available");
    }

    if (!validateLayers(validationLayers, context))
    {
        throw std::runtime_error("Requested validation layers not available");
    }

    auto createInfo = vk::InstanceCreateInfo{};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    instance_ = vk::raii::Instance(context, createInfo);
}

void Instance::createDebugMessenger()
{
    constexpr auto severityFlags = (vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                    | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                    | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    constexpr auto messageTypeFlags = (vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                       | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                                       | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    auto debugCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{};
    debugCreateInfo.messageSeverity = severityFlags;
    debugCreateInfo.messageType = messageTypeFlags;
    debugCreateInfo.pfnUserCallback = &debugCallback;

    debugMessenger_ = instance_.createDebugUtilsMessengerEXT(debugCreateInfo);
}
} // namespace renderer
