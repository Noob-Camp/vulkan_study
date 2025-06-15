add_executable(hello_compute_shader hello_compute_shader.cpp)

target_include_directories(
    hello_compute_shader PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(hello_compute_shader PUBLIC ${Vulkan_LIBRARIES})
