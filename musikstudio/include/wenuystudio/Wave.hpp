// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
namespace wenuystudio {
struct Stereo { float l=0.0f, r=0.0f; };
// Streaming RIFF/WAVE reader. No full-file loading, bounded memory.
class WaveReader {
public:
    explicit WaveReader(const std::string& path);
    WaveReader(const WaveReader&)=delete;
    WaveReader& operator=(const WaveReader&)=delete;
    WaveReader(WaveReader&&)=default;
    std::uint32_t rate() const noexcept { return rate_; }
    std::uint64_t frames() const noexcept { return frames_; }
    void read(std::uint64_t start, Stereo* dst, std::size_t count);
private:
    std::ifstream stream_;
    std::uint64_t dataOffset_=0, frames_=0;
    std::uint32_t rate_=0;
    std::uint16_t format_=0, channels_=0, bits_=0;
};
class WaveWriter {
public:
    WaveWriter(const std::string& path, std::uint32_t sampleRate);
    WaveWriter(const WaveWriter&)=delete;
    WaveWriter& operator=(const WaveWriter&)=delete;
    ~WaveWriter();
    void append(const Stereo* frames, std::size_t count);
    void finish();
private:
    std::ofstream stream_;
    std::uint32_t frames_=0;
    bool finished_=false;
};
}
