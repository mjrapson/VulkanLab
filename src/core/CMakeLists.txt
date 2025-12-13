add_library(Core STATIC)

target_sources(Core
    PRIVATE
        src/file_system.cpp
    PUBLIC
        include/core/file_system.h
)

target_include_directories(Core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)