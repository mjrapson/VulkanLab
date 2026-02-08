add_library(Renderer STATIC)

target_sources(Renderer
    PUBLIC
        include/renderer/camera.h
        include/renderer/descriptor_set_allocator.h
        include/renderer/draw_command.h
        include/renderer/gpu_device.h
        include/renderer/gpu_image.h
        include/renderer/gpu_material.h
        include/renderer/gpu_mesh.h
        include/renderer/gpu_resource_cache.h
        include/renderer/instance.h
        include/renderer/renderer.h
        include/renderer/swapchain.h
        include/renderer/vertex_layout.h
    PRIVATE
        src/render_passes/geometry_pass.cpp
        src/render_passes/geometry_pass.h
        src/render_passes/render_pass_command_info.h
        src/render_passes/skybox_pass.cpp
        src/render_passes/skybox_pass.h
        src/camera.cpp
        src/descriptor_set_allocator.cpp
        src/gpu_device.cpp
        src/gpu_resource_cache.cpp
        src/instance.cpp
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
        Window
        Assets
        spdlog
        Vulkan::Vulkan
        pch
)

target_precompile_headers(Renderer REUSE_FROM pch)
