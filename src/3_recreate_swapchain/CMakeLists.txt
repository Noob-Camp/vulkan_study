add_executable(3_recreate_swapchain 3_recreate_swapchain.cpp)

target_include_directories(
    3_recreate_swapchain PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(
    3_recreate_swapchain PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
