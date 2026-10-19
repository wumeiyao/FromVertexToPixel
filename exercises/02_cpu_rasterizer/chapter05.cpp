#include "image.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

struct Vertex {
    float x;
    float y;
    codea::Color color;
};

float edge(const Vertex& a, const Vertex& b, float x, float y) {
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

void drawTriangle(codea::Image& image, Vertex a, Vertex b, Vertex c) {
    float area = edge(a, b, c.x, c.y);
    if (area < 0.0f) {
        std::swap(b, c);
        area = -area;
    }
    if (area < 1e-6f) {
        return;
    }

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
    const int maxX = std::min(image.width() - 1,
                              static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
    const int maxY = std::min(image.height() - 1,
                              static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = x + 0.5f;
            const float py = y + 0.5f;
            const float wa = edge(b, c, px, py) / area;
            const float wb = edge(c, a, px, py) / area;
            const float wc = edge(a, b, px, py) / area;
            if (wa >= 0.0f && wb >= 0.0f && wc >= 0.0f) {
                image.set(x, y, a.color * wa + b.color * wb + c.color * wc);
            }
        }
    }
}

int main() {
    codea::Image image{512, 512, {0.04f, 0.05f, 0.08f}};
    drawTriangle(image,
                 {256.0f, 48.0f, {1.0f, 0.1f, 0.1f}},
                 {64.0f, 448.0f, {0.1f, 1.0f, 0.2f}},
                 {448.0f, 448.0f, {0.1f, 0.3f, 1.0f}});

    const auto path = codea::outputPath("chapter05_triangle.ppm");
    image.writePpm(path);
    std::cout << "Wrote " << path << " (barycentric color interpolation)\n";
    return 0;
}
