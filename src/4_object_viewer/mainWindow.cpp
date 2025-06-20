#include <mainWindow.hpp>
#include <iostream>


namespace RealTimeBox {

MainWindow::MainWindow(size_t width_, size_t height_, std::string name_)
    : width { width_ }
    , height { height_ }
    , windowName(std::move(name_))
{
    initWindow();
}

MainWindow::~MainWindow() {
    glfwDestroyWindow(glfwWindow);
    glfwTerminate();
}

void MainWindow::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);// 禁止 OpenGL API
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);// 允许窗口缩放

    glfwWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
    // 将用户指针与指定的窗口关联起来
    // 通常用于将自定义的用户数据附加到窗口对象上
    // 处理事件或回调时，就可以通过用户指针来获取和操作相关数据
    glfwSetWindowUserPointer(glfwWindow, this);
    //  注册窗口的帧缓冲大小变化的回调函数
    glfwSetFramebufferSizeCallback(glfwWindow, framebufferResizeCallback);
}

void MainWindow::createWindowSurface(VkInstance instance_, VkSurfaceKHR *surface_) {
    if (glfwCreateWindowSurface(instance_, glfwWindow, nullptr, surface_) != VK_SUCCESS) {
        std::cout << "failed to create window surface!" << std::endl;
    }
}

void MainWindow::framebufferResizeCallback(GLFWwindow * glfwWindow_, int width_, int height_) {
    auto mainWindow = reinterpret_cast<MainWindow *>(glfwGetWindowUserPointer(glfwWindow_));
    mainWindow->framebufferResized = true;
    mainWindow->width = width_;
    mainWindow->height = height_;
}

} // namespace RealTimeBox
