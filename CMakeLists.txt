cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()
if (NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

project(HelloVulkan
    LANGUAGES C CXX
    DESCRIPTION "Learn Vulkan."
)


# GLFW
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/external/glfw) # set this to point to an up-to-date GLFW repo
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)
add_subdirectory(${GLFW_DIR} glfw_binary EXCLUDE_FROM_ALL)

# glm
set(GLM_DIR ${CMAKE_SOURCE_DIR}/external/glm)

# Vulkan
find_package(Vulkan REQUIRED)


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")


# add_subdirectory(src/HelloWindow)
# add_subdirectory(src/Triangle)
# add_subdirectory(src/recreateSwapChain)
add_subdirectory(src/view3DObject)




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
