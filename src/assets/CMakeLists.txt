add_library(Assets STATIC)

target_sources(Assets
    PUBLIC
        include/assets/loaders/image_loader.h
        include/assets/database.h
        include/assets/image_data.h
        include/assets/image.h
        include/assets/loading_screen.h
        include/assets/material.h
        include/assets/mesh.h
        include/assets/prefab.h
        include/assets/skybox.h
    PRIVATE
        src/loaders/image_loader.cpp
        src/database.cpp
        src/image.cpp
        src/loading_screen.cpp
        src/material.cpp
        src/mesh.cpp
        src/skybox.cpp
)

target_include_directories(Assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(Assets
    PRIVATE
        Core
        Renderer
        pch
)

target_compile_definitions(Assets
    PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

target_precompile_headers(Assets REUSE_FROM pch)
