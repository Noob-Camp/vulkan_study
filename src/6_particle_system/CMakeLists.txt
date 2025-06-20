add_executable(6_particle_system 6_particle_system.cpp)

target_include_directories(
    6_particle_system PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(
    6_particle_system PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
