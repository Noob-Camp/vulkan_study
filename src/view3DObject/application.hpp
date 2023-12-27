#ifndef APPLICATION_H_
#define APPLICATION_H_

#include <memory>
#include <vector>

#include <descriptors.hpp>
#include <device.hpp>
#include <gameObject.hpp>
#include <renderer.hpp>
#include <mainWindow.hpp> 


namespace RealTimeBox {
    using namespace std::literals::string_literals;

struct Application {
    static constexpr size_t WIDTH { 800 };
    static constexpr size_t HEIGHT { 600 };

    Application();
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    ~Application();

    void run();

private:
    void loadGameObjects();

    MainWindow mainWindow { WIDTH, HEIGHT, "Hello Vulkan"s };
    Device device { mainWindow };
    Renderer renderer { mainWindow, device };

    // note: order of declarations matters
    std::unique_ptr<DescriptorPool> globalPool;
    GameObject::Map gameObjects;
};
} // namespace RealTimeBox

#endif// APPLICATION_H_
