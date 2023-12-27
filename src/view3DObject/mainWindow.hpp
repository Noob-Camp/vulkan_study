#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


namespace RealTimeBox {
    using namespace std::literals::string_literals;

struct MainWindow {
    MainWindow(size_t width_, size_t height_, std::string name_);
    MainWindow(const MainWindow &) = delete;
    MainWindow &operator=(const MainWindow &) = delete;
    ~MainWindow();

    VkExtent2D getExtent() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }; }
    GLFWwindow *getGLFWwindow() const { return glfwWindow; }
    bool wasWindowResized() { return framebufferResized; }
    void resetWindowResizedFlag() { framebufferResized = false; }

    bool shouldClose() { return glfwWindowShouldClose(glfwWindow); }
    void createWindowSurface(VkInstance instance_, VkSurfaceKHR *surface_);

private:
    static void framebufferResizeCallback(GLFWwindow * glfwWindow_, int width_, int height_);
    void initWindow();

    size_t width { 800 };
    size_t height { 600 };
    std::string windowName { "Hello Vulkan"s };
    GLFWwindow* glfwWindow { nullptr };
    bool framebufferResized = false;
};

} // namespace RealTimeBox

#endif// MAINWINDOW_H_