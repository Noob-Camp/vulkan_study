add_executable(helloComputeShader helloComputeShader.cpp)

target_include_directories(
    helloComputeShader PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    helloComputeShader PUBLIC
    ${Vulkan_LIBRARIES}
)

