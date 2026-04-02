add_library(Window STATIC)

target_sources(Window
    PUBLIC
        include/window/window.h
    PRIVATE
        src/window.cpp
)

target_include_directories(Window
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Window
    PRIVATE
        Core
        glfw
        spdlog
        Vulkan::Vulkan
)

target_compile_definitions(Window
    PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)