add_executable(path_tracing path_tracing.cpp)

target_include_directories(
    path_tracing PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(path_tracing PUBLIC ${Vulkan_LIBRARIES})
