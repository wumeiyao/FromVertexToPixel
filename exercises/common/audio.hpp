#pragma once

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace codea {

inline void writeLittleEndian(std::ofstream& file, std::uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        const auto byte = static_cast<char>((value >> (i * 8)) & 0xffu);
        file.write(&byte, 1);
    }
}

inline void generateToneWav(const std::filesystem::path& path) {
    constexpr int sampleRate = 44100;
    constexpr float duration = 0.12f;
    constexpr float frequency = 880.0f;
    constexpr float pi = 3.14159265358979323846f;
    const int sampleCount = static_cast<int>(sampleRate * duration);
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(sampleCount * 2);

    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot create generated audio file: " + path.string());
    }
    file.write("RIFF", 4);
    writeLittleEndian(file, 36u + dataBytes, 4);
    file.write("WAVEfmt ", 8);
    writeLittleEndian(file, 16, 4);
    writeLittleEndian(file, 1, 2);
    writeLittleEndian(file, 1, 2);
    writeLittleEndian(file, sampleRate, 4);
    writeLittleEndian(file, sampleRate * 2, 4);
    writeLittleEndian(file, 2, 2);
    writeLittleEndian(file, 16, 2);
    file.write("data", 4);
    writeLittleEndian(file, dataBytes, 4);
    for (int i = 0; i < sampleCount; ++i) {
        const float envelope = 1.0f - static_cast<float>(i) / sampleCount;
        const float wave = std::sin(2.0f * pi * frequency * i / sampleRate);
        const auto sample = static_cast<std::int16_t>(wave * envelope * 12000.0f);
        writeLittleEndian(file, static_cast<std::uint16_t>(sample), 2);
    }
}

class AudioSystem {
public:
    explicit AudioSystem(const std::filesystem::path& soundPath) {
        if (!std::filesystem::exists(soundPath)) {
            generateToneWav(soundPath);
        }
#ifdef __APPLE__
        const std::string path = soundPath.string();
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path.data()),
            static_cast<CFIndex>(path.size()), false);
        if (!url || AudioServicesCreateSystemSoundID(url, &sound_) != kAudioServicesNoError) {
            if (url) CFRelease(url);
            throw std::runtime_error("Cannot preload generated WAV: " + path);
        }
        CFRelease(url);
#else
        (void)soundPath;
#endif
    }

    ~AudioSystem() {
#ifdef __APPLE__
        if (sound_ != 0) AudioServicesDisposeSystemSoundID(sound_);
#endif
    }

    void playOneShot() const {
#ifdef __APPLE__
        AudioServicesPlaySystemSound(sound_);
#else
        std::fputc('\a', stdout);
#endif
    }

private:
#ifdef __APPLE__
    SystemSoundID sound_ = 0;
#endif
};

} // namespace codea
