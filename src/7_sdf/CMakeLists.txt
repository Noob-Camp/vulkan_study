add_executable(7_sdf 7_sdf.cpp)

target_include_directories(
    7_sdf PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(
    7_sdf PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
