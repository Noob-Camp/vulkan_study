add_executable(test_glfw test_glfw.cpp)

target_include_directories(
    test_glfw
    PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    test_glfw
    PUBLIC
    ${Vulkan_LIBRARIES}
    glfw
)
