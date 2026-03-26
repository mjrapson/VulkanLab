add_library(Scene STATIC)

target_sources(Scene
    PUBLIC
        include/scene/scene_loader.h
        include/scene/scene.h
    PRIVATE
        src/scene_loader.cpp
)

target_include_directories(Scene
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/nlohmann/include
)

target_link_libraries(Scene
    PRIVATE
        Core
        spdlog
        pch
)

target_compile_definitions(Scene
    PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

target_precompile_headers(Scene REUSE_FROM pch)
