#include "image.hpp"

#include <iostream>

int main() {
    constexpr int size = 512;
    codea::Image image{size, size};

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float u = static_cast<float>(x) / (size - 1);
            const float v = static_cast<float>(y) / (size - 1);
            const bool checker = ((x / 32) + (y / 32)) % 2 == 0;
            const float shade = checker ? 1.0f : 0.35f;
            image.set(x, y, {u * shade, v * shade, 0.25f * shade});
        }
    }

    const auto path = codea::outputPath("chapter04_framebuffer.ppm");
    image.writePpm(path);
    std::cout << "Wrote " << path << " (512x512 RGB framebuffer)\n";
    return 0;
}
