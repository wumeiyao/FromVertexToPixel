#include "image.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

struct Point {
    float x;
    float y;
    float z;
};

std::size_t baselineVisible(const std::vector<Point>& points, float radius) {
    std::size_t count = 0;
    for (const Point& point : points) {
        const float distance = std::sqrt(point.x * point.x + point.y * point.y +
                                         point.z * point.z);
        if (distance < radius) ++count;
    }
    return count;
}

std::size_t optimizedVisible(const std::vector<Point>& points, float radius) {
    const float radiusSquared = radius * radius;
    std::size_t count = 0;
    for (const Point& point : points) {
        const float distanceSquared = point.x * point.x + point.y * point.y +
                                      point.z * point.z;
        if (distanceSquared < radiusSquared) ++count;
    }
    return count;
}

struct Statistics {
    double average;
    double median;
    double p95;
    double p99;
    double maximum;
};

Statistics summarize(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const auto percentile = [&](double p) {
        const std::size_t index = static_cast<std::size_t>(
            std::ceil(p * static_cast<double>(samples.size())) - 1.0);
        return samples[std::min(index, samples.size() - 1)];
    };
    return {
        std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size(),
        percentile(0.50), percentile(0.95), percentile(0.99), samples.back()};
}

template <typename Function>
std::vector<double> measure(const std::vector<Point>& points, Function function,
                            volatile std::size_t& resultSink) {
    constexpr int sampleCount = 120;
    constexpr int repetitionsPerSample = 10;
    std::vector<double> samples;
    samples.reserve(sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        for (int repetition = 0; repetition < repetitionsPerSample; ++repetition) {
            resultSink = function(points, 50.0f);
        }
        const auto end = std::chrono::steady_clock::now();
        const double milliseconds =
            std::chrono::duration<double, std::milli>(end - start).count() /
            repetitionsPerSample;
        samples.push_back(milliseconds);
    }
    return samples;
}

void printStats(const char* name, const Statistics& stats) {
    std::cout << std::left << std::setw(10) << name
              << " avg=" << std::setw(9) << stats.average
              << " median=" << std::setw(9) << stats.median
              << " p95=" << std::setw(9) << stats.p95
              << " p99=" << std::setw(9) << stats.p99
              << " max=" << stats.maximum << " ms\n";
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string{argv[1]} == "--check") {
        const std::vector<Point> points{{0, 0, 0}, {3, 4, 0}, {100, 0, 0}};
        const auto baseline = baselineVisible(points, 50.0f);
        const auto optimized = optimizedVisible(points, 50.0f);
        std::cout << "chapter19 check: baseline=" << baseline
                  << " optimized=" << optimized << '\n';
        return baseline == optimized ? 0 : 1;
    }

    constexpr std::size_t objectCount = 200000;
    std::mt19937 random{20260820};
    std::uniform_real_distribution<float> coordinate{-100.0f, 100.0f};
    std::vector<Point> points(objectCount);
    for (Point& point : points) {
        point = {coordinate(random), coordinate(random), coordinate(random)};
    }

    const std::size_t baselineCount = baselineVisible(points, 50.0f);
    const std::size_t optimizedCount = optimizedVisible(points, 50.0f);
    if (baselineCount != optimizedCount) {
        std::cerr << "Correctness check failed: visible counts differ\n";
        return 1;
    }

    volatile std::size_t resultSink = 0;
    measure(points, baselineVisible, resultSink);
    measure(points, optimizedVisible, resultSink);
    const std::vector<double> baselineSamples = measure(points, baselineVisible, resultSink);
    const std::vector<double> optimizedSamples = measure(points, optimizedVisible, resultSink);
    const Statistics baseline = summarize(baselineSamples);
    const Statistics optimized = summarize(optimizedSamples);

    std::cout << std::fixed << std::setprecision(6)
              << "chapter19 reproducible CPU culling experiment\n"
              << "objects=" << objectCount << " visible=" << baselineCount
              << " samples=" << baselineSamples.size() << "\n";
    printStats("baseline", baseline);
    printStats("optimized", optimized);
    std::cout << "speedup by average = " << baseline.average / optimized.average << "x\n";

    const auto csvPath = codea::outputPath("chapter19_profile.csv");
    std::filesystem::create_directories(csvPath.parent_path());
    std::ofstream csv(csvPath);
    csv << "sample,baseline_ms,optimized_ms\n";
    for (std::size_t i = 0; i < baselineSamples.size(); ++i) {
        csv << i << ',' << baselineSamples[i] << ',' << optimizedSamples[i] << '\n';
    }
    std::cout << "Wrote raw samples to " << csvPath << '\n';
    return 0;
}
