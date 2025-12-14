cmake_minimum_required(VERSION 3.28)

project(VulkanDemo
    VERSION 0.1.0
    DESCRIPTION "Vulkan-based rendering and simulation demo"
    LANGUAGES CXX
)

# -------------------------
# Global C++ settings
# -------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# -------------------------
# Warning policy
# -------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    add_compile_options(/W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# -------------------------
# Output directories
# -------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# -------------------------
# Third-party libraries
# -------------------------
add_subdirectory(third_party/glfw)
add_subdirectory(third_party/glm)
add_subdirectory(third_party/spdlog)

find_package(Vulkan REQUIRED)
find_program(SLANGC_EXECUTABLE
    NAMES slangc
    REQUIRED
)

# -------------------------
# Project modules
# -------------------------
add_subdirectory(src/core)
add_subdirectory(src/renderer)
add_subdirectory(src/main)