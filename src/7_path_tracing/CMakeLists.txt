add_executable(7_path_tracing 7_path_tracing.cpp)

target_include_directories(
    7_path_tracing PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(
    7_path_tracing PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
