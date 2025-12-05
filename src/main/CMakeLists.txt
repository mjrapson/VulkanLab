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
    glfw
    spdlog
    Vulkan::Vulkan
)