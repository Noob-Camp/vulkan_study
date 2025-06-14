add_executable(
    2_triangle
    2_triangle.cpp
)

target_include_directories(
    2_triangle
    PUBLIC
    ${GLFW_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    2_triangle
    PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
