add_library(Assets STATIC)

target_sources(Assets
    PRIVATE
        src/private/gltf_loader.cpp
        src/private/gltf_loader.h
        src/private/image_loader.cpp
        src/private/image_loader.h
        src/asset_database.cpp
        src/image.cpp
        src/prefab.cpp
    PUBLIC
        include/assets/asset_database.h
        include/assets/asset_handle.h
        include/assets/asset_storage.h
        include/assets/image.h
        include/assets/material.h
        include/assets/mesh.h
        include/assets/prefab.h
)

target_include_directories(Assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/stb
        ${CMAKE_SOURCE_DIR}/third_party/tinygltf
)


target_link_libraries(Assets
    PRIVATE
        Core
        spdlog
        pch
)

target_precompile_headers(Assets REUSE_FROM pch)
