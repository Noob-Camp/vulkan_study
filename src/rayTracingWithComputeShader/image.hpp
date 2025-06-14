#pragma once

#include <vector>
#include <glm/glm.hpp>


struct Image {
    std::size_t width;
    std::size_t height;
    bool gammaCorrectOnOutput { true };
    std::vector<glm::vec4> imageData;


    Image() = default;
    Image(std::size_t width, std::size_t height)
        :width(width), height(height)
    {
        imageData.resize(width * height, glm::vec4(0));
    }

    vk::DeviceSize imageSize() { return imageData.size() * sizeof(glm::vec4); }

    friend std::ostream& operator << (std::ostream& out, Image& buffer) {
        out << "P3\n" << buffer.width << ' ' << buffer.height << "\n255\n";
        for (auto pixel : buffer.imageData) {
            pixel.w = 1.0f;
            if (buffer.gammaCorrectOnOutput) { pixel = glm::sqrt(pixel); }

            out << static_cast<int>(256 * glm::clamp(pixel.x, 0.0f, 0.999f)) << ' '
                << static_cast<int>(256 * glm::clamp(pixel.y, 0.0f, 0.999f)) << ' '
                << static_cast<int>(256 * glm::clamp(pixel.z, 0.0f, 0.999f)) << '\n';
        }

        return out;
    }
};
