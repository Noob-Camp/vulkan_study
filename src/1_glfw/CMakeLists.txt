add_executable(1_glfw 1_glfw.cpp)

target_include_directories(
    1_glfw
    PUBLIC
    ${GLFW_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    1_glfw
    PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
