#include "image.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

struct Vec2 {
    float x;
    float y;
};

struct Vertex {
    Vec2 screen;
    float depth;
    float reciprocalW;
    Vec2 uv;
};

float edge(const Vec2& a, const Vec2& b, const Vec2& p) {
    return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

codea::Color sampleChecker(Vec2 uv) {
    uv.x -= std::floor(uv.x);
    uv.y -= std::floor(uv.y);
    const int x = static_cast<int>(uv.x * 8.0f);
    const int y = static_cast<int>(uv.y * 8.0f);
    return ((x + y) & 1) == 0 ? codea::Color{0.95f, 0.85f, 0.15f}
                              : codea::Color{0.08f, 0.15f, 0.35f};
}

void drawTexturedTriangle(codea::Image& image, std::vector<float>& depth,
                          Vertex a, Vertex b, Vertex c) {
    float area = edge(a.screen, b.screen, c.screen);
    if (area < 0.0f) {
        std::swap(b, c);
        area = -area;
    }

    const int minX = std::max(0, static_cast<int>(std::floor(
                                    std::min({a.screen.x, b.screen.x, c.screen.x}))));
    const int maxX = std::min(image.width() - 1, static_cast<int>(std::ceil(
                                    std::max({a.screen.x, b.screen.x, c.screen.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(
                                    std::min({a.screen.y, b.screen.y, c.screen.y}))));
    const int maxY = std::min(image.height() - 1, static_cast<int>(std::ceil(
                                    std::max({a.screen.y, b.screen.y, c.screen.y}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const Vec2 p{x + 0.5f, y + 0.5f};
            const float wa = edge(b.screen, c.screen, p) / area;
            const float wb = edge(c.screen, a.screen, p) / area;
            const float wc = edge(a.screen, b.screen, p) / area;
            if (wa < 0.0f || wb < 0.0f || wc < 0.0f) {
                continue;
            }

            const float candidateDepth = wa * a.depth + wb * b.depth + wc * c.depth;
            const std::size_t index = static_cast<std::size_t>(y * image.width() + x);
            if (candidateDepth >= depth[index]) {
                continue;
            }
            depth[index] = candidateDepth;

            const float inverseW = wa * a.reciprocalW + wb * b.reciprocalW +
                                   wc * c.reciprocalW;
            const Vec2 uvOverW{
                wa * a.uv.x * a.reciprocalW + wb * b.uv.x * b.reciprocalW +
                    wc * c.uv.x * c.reciprocalW,
                wa * a.uv.y * a.reciprocalW + wb * b.uv.y * b.reciprocalW +
                    wc * c.uv.y * c.reciprocalW};
            image.set(x, y, sampleChecker({uvOverW.x / inverseW, uvOverW.y / inverseW}));
        }
    }
}

int main() {
    constexpr int width = 640;
    constexpr int height = 480;
    codea::Image image{width, height, {0.04f, 0.05f, 0.08f}};
    std::vector<float> depth(static_cast<std::size_t>(width * height),
                             std::numeric_limits<float>::infinity());

    drawTexturedTriangle(image, depth,
                         {{90.0f, 80.0f}, 0.7f, 1.0f, {0.0f, 0.0f}},
                         {{560.0f, 170.0f}, 0.7f, 0.35f, {3.0f, 0.0f}},
                         {{320.0f, 440.0f}, 0.7f, 0.5f, {1.5f, 3.0f}});

    drawTexturedTriangle(image, depth,
                         {{180.0f, 130.0f}, 0.3f, 1.0f, {0.0f, 0.0f}},
                         {{470.0f, 130.0f}, 0.3f, 1.0f, {2.0f, 0.0f}},
                         {{325.0f, 350.0f}, 0.3f, 1.0f, {1.0f, 2.0f}});

    const auto path = codea::outputPath("chapter06_depth_texture.ppm");
    image.writePpm(path);
    std::cout << "Wrote " << path
              << " (depth test + perspective-correct texture coordinates)\n";
    return 0;
}
