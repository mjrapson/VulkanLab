add_library(pch STATIC
    pch.cpp
)

target_link_libraries(pch PRIVATE spdlog Vulkan::Vulkan)

target_precompile_headers(pch PUBLIC
    <vulkan/vulkan.hpp>
    <vulkan/vulkan_raii.hpp>

    <spdlog/spdlog.h>

    <glm/gtc/matrix_transform.hpp>
    <glm/glm.hpp>    
    
    <algorithm>
    <array>
    <cassert>
    <chrono>
    <filesystem>
    <fstream>
    <memory>
    <optional>
    <ranges>
    <span>
    <stdexcept>
    <stdint.h>
    <string>
    <tuple>
    <unordered_map>
    <vector>
)
