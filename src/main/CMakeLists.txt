add_executable(VulkanDemo)

target_sources(VulkanDemo
    PRIVATE
    main.cpp
    vulkan_application.cpp
    vulkan_application.h
)

target_link_libraries(VulkanDemo
    PRIVATE
    Core
    Window
    Renderer
    Assets
    Scene
    glfw
    glm
    spdlog
    Vulkan::Vulkan
)

target_compile_definitions(VulkanDemo
    PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/assets/shaders)
set(SHADER_OUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

add_shader(VulkanDemo basic VERTEX_ENTRY vertMain FRAGMENT_ENTRY fragMain OUTPUT_DIR ${SHADER_OUT_DIR})
add_shader(VulkanDemo shadowmap VERTEX_ENTRY vertMain FRAGMENT_ENTRY fragMain OUTPUT_DIR ${SHADER_OUT_DIR})
add_shader(VulkanDemo skybox VERTEX_ENTRY vertMain FRAGMENT_ENTRY fragMain OUTPUT_DIR ${SHADER_OUT_DIR})
add_shader(VulkanDemo loading_screen VERTEX_ENTRY vertMain FRAGMENT_ENTRY fragMain OUTPUT_DIR ${SHADER_OUT_DIR})

add_custom_target(post_build ALL 
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/prefabs ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/prefabs
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/scenes ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scenes
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/textures ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/textures
)

add_dependencies(post_build VulkanDemo)

