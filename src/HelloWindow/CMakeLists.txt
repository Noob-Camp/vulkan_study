add_executable(HelloWindow HelloWindow.cpp)

target_include_directories(
    HelloWindow PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    HelloWindow PUBLIC
    ${Vulkan_LIBRARIES}
    glfw
)
