add_executable(Triangle Triangle.cpp)

target_include_directories(
    Triangle PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    Triangle PUBLIC
    ${Vulkan_LIBRARIES}
    glfw
)
