file(GLOB_RECURSE view3DObject_sources CONFIGURE_DEPENDS ./*.cpp)
file(GLOB_RECURSE view3DObject_headers CONFIGURE_DEPENDS ./*.hpp)
add_executable(view3DObject ${view3DObject_sources} ${view3DObject_headers})

target_include_directories(
    view3DObject PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    view3DObject PUBLIC
    ${Vulkan_LIBRARIES}
    glfw
)
