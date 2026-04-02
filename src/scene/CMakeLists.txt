add_library(Scene STATIC)

target_sources(Scene
    PUBLIC
        include/scene/systems/render_system.h
        include/scene/entity.h
        include/scene/scene_gltf_loader.h
        include/scene/scene_loader.h
        include/scene/scene.h
    PRIVATE
        src/systems/render_system.cpp
        src/scene_gltf_loader.cpp
        src/scene_loader.cpp
)

target_include_directories(Scene
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${tinygltf_SOURCE_DIR}
)

target_link_libraries(Scene
    PRIVATE
        Core
        Assets
        Renderer
        glm
        spdlog
        Vulkan::Vulkan
)

target_compile_definitions(Scene
    PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)