#pragma once

#include <string>
#include <vector>

#ifdef NDEBUG
    constexpr auto enable_validation_layers { false };
#else
    constexpr auto enable_validation_layers { true };
#endif

using namespace std::literals::string_literals;


struct WindowData {
    int width { 800 };
    int height { 600 };
    std::string name { "Hello Triangle"s };
    bool framebuffer_resized { false };
};

WindowData* window_data = new WindowData();
auto validation_layer_supported { false };
const std::vector<const char*> validation_layers = { "VK_LAYER_KHRONOS_validation" };
