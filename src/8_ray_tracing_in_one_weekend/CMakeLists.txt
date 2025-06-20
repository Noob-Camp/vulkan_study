add_executable(8_ray_tracing_in_one_weekend 8_ray_tracing_in_one_weekend.cpp)

target_include_directories(
    8_ray_tracing_in_one_weekend PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(
    8_ray_tracing_in_one_weekend PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
