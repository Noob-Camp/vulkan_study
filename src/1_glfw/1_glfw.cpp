#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <minilog.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>


namespace {

VkInstance instance;
void handle_input(GLFWwindow* window); // press the ESC key to exit the window
void create_instance(); // create VkInstance for Vulkan

} // namesapce anonymous end


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Hello GLFW Window", nullptr, nullptr);

    // test GLM API
    auto vec = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
    auto mat = glm::mat4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 3.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 4.0f
    );
    auto v = mat * vec;

    std::cout << "vec = " << "("
              << vec.x << ", "
              << vec.y << ", "
              << vec.z << ", "
              << vec.w << ")"
              << std::endl;

    std::cout << "mat = \n" << "["
              << mat[0][0] << ", " << mat[0][1] << ", " << mat[0][2] << ", " << mat[0][3] << ", \n"
              << mat[1][0] << ", " << mat[1][1] << ", " << mat[1][2] << ", " << mat[1][3] << ", \n"
              << mat[2][0] << ", " << mat[2][1] << ", " << mat[2][2] << ", " << mat[2][3] << ", \n"
              << mat[3][0] << ", " << mat[3][1] << ", " << mat[3][2] << ", " << mat[3][3] << "]"
              << std::endl;

    std::cout << "v = mat * vec = " << "("
              << v.x << ", "
              << v.y << ", "
              << v.z << ", "
              << v.w << ")"
              << std::endl;

    // test Vulkan API
    create_instance();

    while (!glfwWindowShouldClose(window)) {
        handle_input(window);
        glfwPollEvents();
    }

    // destruction of resources
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}


namespace {

void handle_input(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void create_instance() {
    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Hello Vulkan",
        .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 3, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions
    };

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        minilog::log_fatal("failed to create VkInstance!");
    }
    minilog::log_info("create Vulkan instance successfully ");
}

} // namesapce anonymous end
