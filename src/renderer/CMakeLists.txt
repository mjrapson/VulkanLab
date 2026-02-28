add_library(Renderer STATIC)

target_sources(Renderer
    PUBLIC
        include/renderer/buffer_object.h
        include/renderer/buffers.h
        include/renderer/camera.h
        include/renderer/data.h
        include/renderer/descriptor_set_allocator.h
        include/renderer/draw_command.h
        include/renderer/frustum.h
        include/renderer/gpu_device.h
        include/renderer/gpu_objects.h
        include/renderer/handles.h
        include/renderer/instance.h
        include/renderer/renderer.h
        include/renderer/resources.h
        include/renderer/swapchain.h
        include/renderer/vertex_layout.h
    PRIVATE
        src/render_passes/geometry_pass.cpp
        src/render_passes/geometry_pass.h
        src/render_passes/loading_screen_pass.cpp
        src/render_passes/loading_screen_pass.h
        src/render_passes/shadow_map_pass.cpp
        src/render_passes/shadow_map_pass.h
        src/render_passes/skybox_pass.cpp
        src/render_passes/skybox_pass.h
        src/descriptor_set_allocator.cpp
        src/gpu_device.cpp
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
        spdlog
        Vulkan::Vulkan
        pch
)

target_precompile_headers(Renderer REUSE_FROM pch)
