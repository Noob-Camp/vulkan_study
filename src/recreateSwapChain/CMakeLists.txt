add_executable(recreateSwapChain main.cpp)

target_include_directories(
    recreateSwapChain PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    recreateSwapChain PUBLIC
    ${Vulkan_LIBRARIES}
    glfw
)
