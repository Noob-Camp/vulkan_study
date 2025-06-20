file(
    GLOB_RECURSE object_viewer_headers
    CONFIGURE_DEPENDS ./src/4_object_viewer/*.hpp
)
file(
    GLOB_RECURSE object_viewer_sources
    CONFIGURE_DEPENDS ./src/4_object_viewer/*.cpp
)
add_executable(
    4_object_viewer
    ${object_viewer_headers}
    ${object_viewer_sources}
)

target_include_directories(
    4_object_viewer PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    4_object_viewer PUBLIC
    glfw
    ${Vulkan_LIBRARIES}
)
