add_executable(VulkanDemo)

target_sources(VulkanDemo
    PRIVATE
    main.cpp
    vulkan_application.cpp
    vulkan_application.h
)

target_include_directories(VulkanDemo
    PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/glfw/include
)

target_link_libraries(VulkanDemo
    PRIVATE
    Core
    Assets
    Renderer
    glfw
    spdlog
    Vulkan::Vulkan
)

set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/assets/shaders)
set(SHADER_OUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)

file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADER_FILENAMES
    basic
)

function(add_shader SHADER_NAME)
    set(SHADER_SOURCE ${SHADER_SRC_DIR}/${SHADER_NAME}.slang)
    set(VERT_OUT ${SHADER_OUT_DIR}/${SHADER_NAME}.vert.spv)
    set(FRAG_OUT ${SHADER_OUT_DIR}/${SHADER_NAME}.frag.spv)
    set(SLANGC_ARGS -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name)

    add_custom_command(
        OUTPUT ${VERT_OUT} 
        COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCE} -entry vertMain ${SLANGC_ARGS} -o ${VERT_OUT} 
        DEPENDS ${SHADER_SOURCE} 
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${FRAG_OUT} 
        COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCE} -entry fragMain ${SLANGC_ARGS} -o ${FRAG_OUT} 
        DEPENDS ${SHADER_SOURCE} 
        VERBATIM
    )

    add_custom_target(shader_${SHADER_NAME} DEPENDS ${VERT_OUT} ${FRAG_OUT})
endfunction()

foreach(SHADER_NAME ${SHADER_FILENAMES})
    add_shader(${SHADER_NAME})
    add_dependencies(VulkanDemo shader_${SHADER_NAME})
endforeach()

add_custom_target(post_build ALL 
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/prefabs ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/prefabs
)

add_dependencies(post_build VulkanDemo)

