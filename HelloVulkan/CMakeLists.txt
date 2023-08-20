cmake_minimum_required(VERSION 3.26)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(HelloVulkan LANGUAGES C CXX)

add_executable(${PROJECT_NAME} testGLM.cpp)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/../libraries/glfw/include
    ${CMAKE_SOURCE_DIR}/../libraries/glm
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    vulkan
    ${CMAKE_SOURCE_DIR}/../libraries/glfw/build-so/src/libglfw.so
)
