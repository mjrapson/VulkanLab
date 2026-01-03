add_library(Assets STATIC)

target_sources(Assets
    PRIVATE
        src/private/image_loader.cpp
        src/private/image_loader.h
        src/asset_database.cpp
        src/image.cpp
    PUBLIC
        include/assets/asset_database.h
        include/assets/asset_handle.h
        include/assets/asset_storage.h
        include/assets/image.h
)

target_include_directories(Assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/stb
)
