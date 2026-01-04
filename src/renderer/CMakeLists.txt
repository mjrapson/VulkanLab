add_library(Renderer STATIC)

target_sources(Renderer
    PUBLIC
        include/renderer/gpu_device.h
        include/renderer/gpu_image.h
        include/renderer/gpu_material.h
        include/renderer/gpu_mesh.h
        include/renderer/gpu_resource_cache.h
        include/renderer/renderer.h
        include/renderer/vertex_layout.h
    PRIVATE
        src/private/buffer.cpp
        src/private/buffer.h
        src/private/image.cpp
        src/private/image.h
        src/private/memory.cpp
        src/private/memory.h
        src/private/shader.cpp
        src/private/shader.h
        src/gpu_device.cpp
        src/gpu_resource_cache.cpp
        src/renderer.cpp
)

target_include_directories(Renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Renderer
    PRIVATE
        Core
        Assets
        spdlog
        Vulkan::Vulkan
        pch
)

target_precompile_headers(Renderer REUSE_FROM pch)
