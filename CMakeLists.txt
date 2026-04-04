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
include(FetchContent)

# ---- GLFW ----
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# ---- GLM ----
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

# ---- spdlog ----
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.3
)
FetchContent_MakeAvailable(spdlog)

# ---- stb (header-only) ----
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        f1c79c02822848a9bed4315b12c8c8f3761e1296
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# ---- tinygltf ----
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL_VENDOR OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v3.0.0
)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG        v3.3.0
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

find_package(Vulkan REQUIRED)
find_program(SLANGC_EXECUTABLE
    NAMES slangc
    REQUIRED
)

# -------------------------
# Project modules
# -------------------------
include(cmake/shader.cmake)

add_subdirectory(src/core)
add_subdirectory(src/window)
add_subdirectory(src/renderer)
add_subdirectory(src/assets)
add_subdirectory(src/scene)
add_subdirectory(src/main)
