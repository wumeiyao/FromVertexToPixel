#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace codea {

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

inline Color operator+(const Color& a, const Color& b) {
    return {a.r + b.r, a.g + b.g, a.b + b.b};
}

inline Color operator*(const Color& color, float value) {
    return {color.r * value, color.g * value, color.b * value};
}

class Image {
public:
    Image(int width, int height, Color clear = {})
        : width_(width), height_(height), pixels_(width * height, clear) {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("Image dimensions must be positive");
        }
    }

    int width() const { return width_; }
    int height() const { return height_; }

    void clear(Color color) {
        std::fill(pixels_.begin(), pixels_.end(), color);
    }

    void set(int x, int y, Color color) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            return;
        }
        pixels_[static_cast<std::size_t>(y * width_ + x)] = color;
    }

    const Color& get(int x, int y) const {
        return pixels_.at(static_cast<std::size_t>(y * width_ + x));
    }

    void writePpm(const std::filesystem::path& path) const {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open output image: " + path.string());
        }

        file << "P6\n" << width_ << ' ' << height_ << "\n255\n";
        for (const Color& color : pixels_) {
            const auto encode = [](float value) {
                value = std::clamp(value, 0.0f, 1.0f);
                return static_cast<std::uint8_t>(std::lround(value * 255.0f));
            };
            const std::uint8_t bytes[] = {
                encode(color.r), encode(color.g), encode(color.b)};
            file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        }
    }

private:
    int width_;
    int height_;
    std::vector<Color> pixels_;
};

inline std::filesystem::path outputPath(const std::string& filename) {
    return std::filesystem::path(CODEA_OUTPUT_DIR) / filename;
}

} // namespace codea
