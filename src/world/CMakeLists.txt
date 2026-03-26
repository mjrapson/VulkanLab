add_library(World STATIC)

target_sources(World
    PUBLIC
        include/world/components/render_component.h
        include/world/components/transform_component.h
        include/world/systems/render_system.h
        include/world/directional_light.h
        include/world/entity.h
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
        Assets
        spdlog
        pch
)

target_compile_definitions(World
    PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

target_precompile_headers(World REUSE_FROM pch)
