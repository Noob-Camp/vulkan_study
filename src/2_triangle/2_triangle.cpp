#include "data.hpp"
#include "init_helper.hpp"

#include <cstdint>
#include <vector>


int main() {
    // Init GLFW Window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* glfw_window = glfwCreateWindow(
        window_data->width,
        window_data->height,
        window_data->name.c_str(),
        nullptr,
        nullptr
    );
    if (glfw_window == nullptr) {
        minilog::log_fatal("GLFW Failed to create GLFWwindow!");
    } else {
        minilog::log_info("GLFW Create GLFWwindow Successfully!");
    }
    glfwSetWindowUserPointer(glfw_window, window_data);
    glfwSetFramebufferSizeCallback(
        glfw_window,
        [](GLFWwindow* window, int width, int height) {
            auto data = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data->width =  width;
            data->height = height;
            data->framebuffer_resized = true;
            minilog::log_info("the window's size is ({0}, {1})", width, height);
        }
    );

    // Init Vulkan
    check_validation_layer_support();
    populateDebugUtilsMessengerCreateInfoEXT();
    create_instance();
    setup_debug_messenger();

    // Create surface
    if (glfwCreateWindowSurface(instance, glfw_window, nullptr, &surface) != VK_SUCCESS) {
        minilog::log_fatal("failed to create VkSurfaceKHR!");
    } else {
        minilog::log_info("create VkSurfaceKHR successfully!");
    }

    pick_physical_device();
    createLogicalDevice();

    createRenderPass();
    createGraphicsPipeline();

    createFrameBuffers();
    createCommandPool();
    createCommandBuffer();

    createSyncObjects();


    // loop
    while (!glfwWindowShouldClose(glfw_window)) {
        glfwPollEvents();
        drawFrame();
    }
    vkDeviceWaitIdle(logicalDevice);

    // Clean
    for (size_t i { 0 }; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }

    vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(logicalDevice, imageView, nullptr);
    }

    vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);

    if (enable_validation_layers) {
        DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(glfw_window);

    glfwTerminate();

    return EXIT_SUCCESS;
}
