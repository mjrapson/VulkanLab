add_library(Assets STATIC)

target_sources(Assets
    PUBLIC
        include/assets/image_data.h
        include/assets/image.h
        include/assets/loading_screen.h
        include/assets/material.h
        include/assets/mesh.h
        include/assets/prefab.h
        include/assets/skybox.h
    PRIVATE
        src/image.cpp
        src/loading_screen.cpp
        src/material.cpp
        src/mesh.cpp
        src/skybox.cpp
)

target_include_directories(Assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Assets
    PRIVATE
        Core
        Renderer
        pch
)

target_precompile_headers(Assets REUSE_FROM pch)
