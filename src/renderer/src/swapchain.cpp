// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#include "renderer/swapchain.h"

namespace renderer
{
vk::Extent2D getSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities,
                                uint32_t windowWidth,
                                uint32_t windowHeight)
{
    if (capabilities.currentExtent.width != 0xFFFFFFFF)
    {
        return capabilities.currentExtent;
    }

    return {std::clamp<uint32_t>(windowWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(windowHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

uint32_t getSurfaceMinImageCount(uint32_t preferredCount, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities)
{
    if (surfaceCapabilities.maxImageCount == 0) // no maximum
    {
        return std::max(preferredCount, surfaceCapabilities.minImageCount);
    }

    return std::clamp(preferredCount, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
}

vk::SurfaceFormatKHR getSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& surfaceFormats)
{
    if (surfaceFormats.empty())
    {
        throw std::runtime_error("No available surface formats");
    }

    if (auto itr = std::ranges::find_if(surfaceFormats,
                                        [](const vk::SurfaceFormatKHR& format)
                                        {
                                            return format.format == vk::Format::eB8G8R8A8Srgb
                                                   && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
                                        });
        itr != surfaceFormats.end())
    {
        return *itr;
    }

    return surfaceFormats.at(0);
}

Swapchain::Swapchain(const vk::raii::Device& device,
                     const vk::raii::PhysicalDevice& physicalDevice,
                     const vk::raii::SurfaceKHR& surface)
    : device_{device},
      physicalDevice_{physicalDevice},
      surface_{surface}
{
}

void Swapchain::initialize(uint32_t preferredFramesInFlight, vk::Extent2D windowExtent)
{
    if (initialized_)
    {
        swapchain_.clear();
        images_.clear();
        imageViews_.clear();
        presentCompleteSemaphores_.clear();
        renderFinishedSemaphores_.clear();
    }

    initialized_ = true;

    if (outOfDate_)
    {
        outOfDate_ = false;
    }

    const auto surfaceCapabilities = physicalDevice_.getSurfaceCapabilitiesKHR(*surface_);
    const auto surfaceFormat = getSurfaceFormat(physicalDevice_.getSurfaceFormatsKHR(*surface_));

    imageFormat_ = surfaceFormat.format;
    extent_ = getSwapchainExtent(surfaceCapabilities, windowExtent.width, windowExtent.height);
    maxFramesInFlight_ = getSurfaceMinImageCount(preferredFramesInFlight, surfaceCapabilities);

    auto swapChainCreateInfo = vk::SwapchainCreateInfoKHR{};
    swapChainCreateInfo.surface = *surface_;
    swapChainCreateInfo.minImageCount = maxFramesInFlight_;
    swapChainCreateInfo.imageFormat = imageFormat_;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent_;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode = vk::PresentModeKHR::eFifo, swapChainCreateInfo.clipped = true;

    swapchain_ = vk::raii::SwapchainKHR(device_, swapChainCreateInfo);
    images_ = swapchain_.getImages();

    auto subresourceRange = vk::ImageSubresourceRange{};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = surfaceFormat.format;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    for (const auto& image : images_)
    {
        imageViewCreateInfo.image = image;
        imageViews_.emplace_back(device_, imageViewCreateInfo);
    }

    for (auto i = size_t{0}; i < maxFramesInFlight_; ++i)
    {
        renderFinishedSemaphores_.emplace_back(device_, vk::SemaphoreCreateInfo{});
        presentCompleteSemaphores_.emplace_back(device_, vk::SemaphoreCreateInfo{});
    }
}

bool Swapchain::acquireNextImage()
{
    try
    {
        auto result = vk::Result{};
        auto imageIndex = uint32_t{};

        std::tie(result, imageIndex) = swapchain_.acquireNextImage(std::numeric_limits<uint64_t>::max(),
                                                                   presentCompleteSemaphores_.at(currentFrameIndex_),
                                                                   nullptr);

        currentImageIndex_ = imageIndex;
        return true;
    }
    catch (const vk::OutOfDateKHRError&)
    {
        outOfDate_ = true;
        return false;
    }
}

void Swapchain::present(const vk::raii::Queue& presentQueue)
{
    auto presentInfo = vk::PresentInfoKHR{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &*renderFinishedSemaphores_.at(currentImageIndex_);
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &*swapchain_;
    presentInfo.pImageIndices = &currentImageIndex_;

    try
    {
        const auto result = presentQueue.presentKHR(presentInfo);

        if (result == vk::Result::eSuboptimalKHR)
        {
            outOfDate_ = true;
        }
    }
    catch (const vk::OutOfDateKHRError&)
    {
        outOfDate_ = true;
    }

    currentFrameIndex_ = (currentFrameIndex_ + 1) % maxFramesInFlight_;
}

void Swapchain::markOutOfDate()
{
    outOfDate_ = true;
}

bool Swapchain::outOfDate() const
{
    return outOfDate_;
}

vk::Image Swapchain::currentImage() const
{
    return images_.at(currentImageIndex_);
}

const vk::raii::ImageView& Swapchain::currentImageView() const
{
    return imageViews_.at(currentImageIndex_);
}

const vk::raii::Semaphore& Swapchain::currentPresentCompleteSemaphore() const
{
    return presentCompleteSemaphores_.at(currentFrameIndex_);
}

const vk::raii::Semaphore& Swapchain::currentRenderFinishedSemaphore() const
{
    return renderFinishedSemaphores_.at(currentImageIndex_);
}

vk::Extent2D Swapchain::extent() const
{
    return extent_;
}

vk::Format Swapchain::imageFormat() const
{
    return imageFormat_;
}
} // namespace renderer
