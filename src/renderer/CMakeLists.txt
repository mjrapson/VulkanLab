add_library(Renderer STATIC)

target_sources(Renderer
    PUBLIC
        include/renderer/camera.h
        include/renderer/gpu_device.h
        include/renderer/renderer.h
        include/renderer/vertex_layout.h
    PRIVATE
        src/private/buffer.cpp
        src/private/buffer.h
        src/private/gpu_image.h
        src/private/gpu_material.h
        src/private/gpu_mesh.h
        src/private/gpu_resource_cache.cpp
        src/private/gpu_resource_cache.h
        src/private/image.cpp
        src/private/image.h
        src/private/memory.cpp
        src/private/memory.h
        src/private/shader.cpp
        src/private/shader.h
        src/render_passes/geometry_pass.cpp
        src/render_passes/geometry_pass.h
        src/camera.cpp
        src/gpu_device.cpp
        src/renderer.cpp
)

target_include_directories(Renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
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
