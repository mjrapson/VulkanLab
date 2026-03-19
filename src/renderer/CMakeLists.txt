add_library(Renderer STATIC)

target_sources(Renderer
    PUBLIC
        include/renderer/buffer_object.h
        include/renderer/camera.h
        include/renderer/descriptor_set_allocator.h
        include/renderer/draw_command.h
        include/renderer/frustum.h
        include/renderer/gpu_device.h
        include/renderer/gpu_objects.h
        include/renderer/handles.h
        include/renderer/instance.h
        include/renderer/pipeline.h
        include/renderer/renderer.h
        include/renderer/resources.h
        include/renderer/swapchain.h
        include/renderer/transition_barrier.h
        include/renderer/vertex_layout.h
    PRIVATE
        src/descriptor_set_allocator.cpp
        src/gpu_device.cpp
        src/pipeline.cpp
        src/instance.cpp
        src/renderer.cpp
        src/swapchain.cpp
        src/transition_barrier.cpp
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
