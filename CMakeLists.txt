cmake_minimum_required(VERSION 4.0.3)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()
if (NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

project(
    HelloVulkan
    LANGUAGES C CXX
    DESCRIPTION "Learn Vulkan step by step."
)


# GLFW
find_package(glfw3 3.3 REQUIRED)

# glm
find_package(glm REQUIRED)

# Vulkan
find_package(Vulkan REQUIRED)


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")


add_subdirectory(src/1_glfw)
add_subdirectory(src/2_triangle)
add_subdirectory(src/recreateSwapChain)
add_subdirectory(src/hello_compute_shader)
# add_subdirectory(src/view3DObject)
# add_subdirectory(src/rayTracingWithComputeShader)




# # get a list of all subdirectories
# file(GLOB_RECURSE SUBDIRS RELATIVE ${CMAKE_SOURCE_DIR}/src/*)
# foreach(SUBDIR ${SUBDIRS})
#     # set the output path for sub projects
#     set_target_properties(${SUBDIR} PROPERTIES
#         RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${SUBDIR}"
#         LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${SUBDIR}"
#     )
#     add_subdirectory(src/${SUBDIR})
# endforeach()
