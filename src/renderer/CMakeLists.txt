add_library(Renderer STATIC)

target_sources(Renderer
    PUBLIC
        include/renderer/gpu_device.h
        include/renderer/renderer.h
        include/renderer/vertex.h
    PRIVATE
        src/private/buffer.cpp
        src/private/buffer.h
        src/private/memory.cpp
        src/private/memory.h
        src/private/shader.cpp
        src/private/shader.h
        src/gpu_device.cpp
        src/renderer.cpp
)

target_include_directories(Renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Renderer
    PRIVATE
    Core
    spdlog
    Vulkan::Vulkan
)