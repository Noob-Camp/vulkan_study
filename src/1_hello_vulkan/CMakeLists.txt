add_executable(1_hello_vulkan 1_hello_vulkan.cpp)

target_include_directories(
    1_hello_vulkan PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(
    1_hello_vulkan PUBLIC
    glfw
    glm::glm
    ${Vulkan_LIBRARIES}
)
