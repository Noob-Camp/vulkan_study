#include <GLFW/glfw3.h>
#include <iostream>

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Hello Window", nullptr, nullptr);

    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        glfwPollEvents();
        std::cout << "Progamme is running now, the time is: " << glfwGetTime()  << std::endl;
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
