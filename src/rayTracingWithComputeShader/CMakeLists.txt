add_executable(rayTracingWithComputeShader main.cpp)

target_include_directories(
    rayTracingWithComputeShader PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(
    rayTracingWithComputeShader PUBLIC
    ${Vulkan_LIBRARIES}
    glfw
)
