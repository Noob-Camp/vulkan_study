add_executable(5_hello_compute_shader 5_hello_compute_shader.cpp)

target_include_directories(
    5_hello_compute_shader PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(5_hello_compute_shader PUBLIC ${Vulkan_LIBRARIES})
