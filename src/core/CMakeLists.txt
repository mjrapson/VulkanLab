add_library(Core STATIC)

target_sources(Core
    PUBLIC
        include/core/box.h
        include/core/file_system.h
        include/core/handle.h
        include/core/input_handler.h
        include/core/memory.h
        include/core/vertex.h
    PRIVATE
        src/file_system.cpp
        src/input_handler.cpp
)

target_include_directories(Core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Core
    PRIVATE
        pch
)

target_compile_definitions(Core
    PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

target_precompile_headers(Core REUSE_FROM pch)
