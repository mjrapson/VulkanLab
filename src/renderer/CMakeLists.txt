add_library(Renderer STATIC)

target_sources(Renderer
    PRIVATE
        src/gpu_device.cpp
    PUBLIC
        include/renderer/gpu_device.h
)

target_include_directories(Renderer
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Renderer
    PRIVATE
    spdlog
    Vulkan::Vulkan
)