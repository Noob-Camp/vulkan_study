add_executable(2_hello_render_shader 2_hello_render_shader.cpp)

target_include_directories(
    2_hello_render_shader PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${GLFW_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(
    2_hello_render_shader PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
