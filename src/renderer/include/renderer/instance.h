/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace renderer
{
class Instance
{
  public:
    Instance(const vk::raii::Context& context, std::span<const char* const> windowExtensions);

    const vk::raii::Instance& instance() const;

  private:
    void createInstance(const vk::raii::Context& context, std::span<const char* const> windowExtensions);
    void createDebugMessenger();

  private:
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debugMessenger_{nullptr};
};
} // namespace renderer
