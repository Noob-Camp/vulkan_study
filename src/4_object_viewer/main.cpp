#include <application.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    RealTimeBox::Application app {};

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
