add_library(World STATIC)

target_sources(World
    PUBLIC
        include/world/components/camera_component.h
        include/world/components/render_component.h
        include/world/components/transform_component.h
        include/world/systems/render_system.h
        include/world/entity.h
        include/world/world.h
    PRIVATE
        src/components/camera_component.cpp
        src/systems/render_system.cpp
)

target_include_directories(World
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(World
    PRIVATE
        Core
        Assets
        Renderer
        spdlog
        pch
)

target_precompile_headers(World REUSE_FROM pch)
