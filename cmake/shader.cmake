function(add_shader TARGET_NAME SHADER_NAME)
    cmake_parse_arguments(
        SHADER
        ""
        "OUTPUT_DIR"
        "VERTEX_ENTRY;FRAGMENT_ENTRY;COMPUTE_ENTRY"
        ${ARGN}
    )

    set(SHADER_SOURCE ${SHADER_SRC_DIR}/${SHADER_NAME}.slang)

    if(NOT SHADER_OUTPUT_DIR)
        message(FATAL_ERROR
            "add_shader(${SHADER_NAME}): OUTPUT_DIR is required")
    endif()

    set(OUTPUT_DIR ${SHADER_OUTPUT_DIR})

    set(SLANGC_ARGS
        -target spirv
        -profile spirv_1_4
        -emit-spirv-directly
        -fvk-use-entrypoint-name
    )

    set(OUTPUTS)

    # Vertex
    foreach(ENTRY IN LISTS SHADER_VERTEX_ENTRY)
        set(OUT_FILE ${OUTPUT_DIR}/${SHADER_NAME}.vertex.spv)
        add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCE}
                    -entry ${ENTRY} ${SLANGC_ARGS}
                    -o ${OUT_FILE}
            DEPENDS ${SHADER_SOURCE}
            VERBATIM
        )
        list(APPEND OUTPUTS ${OUT_FILE})
    endforeach()

    # Fragment
    foreach(ENTRY IN LISTS SHADER_FRAGMENT_ENTRY)
        set(OUT_FILE ${OUTPUT_DIR}/${SHADER_NAME}.fragment.spv)
        add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCE}
                    -entry ${ENTRY} ${SLANGC_ARGS}
                    -o ${OUT_FILE}
            DEPENDS ${SHADER_SOURCE}
            VERBATIM
        )
        list(APPEND OUTPUTS ${OUT_FILE})
    endforeach()

    # Compute
    foreach(ENTRY IN LISTS SHADER_COMPUTE_ENTRY)
        set(OUT_FILE ${OUTPUT_DIR}/${SHADER_NAME}.compute.spv)
        add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCE}
                    -entry ${ENTRY} ${SLANGC_ARGS}
                    -o ${OUT_FILE}
            DEPENDS ${SHADER_SOURCE}
            VERBATIM
        )
        list(APPEND OUTPUTS ${OUT_FILE})
    endforeach()

    add_custom_target(shader_${SHADER_NAME} DEPENDS ${OUTPUTS})
    add_dependencies(${TARGET_NAME} shader_${SHADER_NAME})
endfunction()
