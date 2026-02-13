add_library(World STATIC)

target_sources(World
    PUBLIC
        include/world/components/directional_light_component.h
        include/world/components/render_component.h
        include/world/components/transform_component.h
        include/world/systems/render_system.h
        include/world/entity.h
        include/world/prefab.h
        include/world/world.h
    PRIVATE
        src/systems/render_system.cpp
        src/world.cpp
)

target_include_directories(World
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(World
    PRIVATE
        Core
        Scene
        Renderer
        spdlog
        pch
)

target_precompile_headers(World REUSE_FROM pch)
