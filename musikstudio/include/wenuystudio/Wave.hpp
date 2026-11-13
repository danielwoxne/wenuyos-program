// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
namespace wenuystudio {
struct Stereo { float l=0.f,r=0.f; };
class WaveReader {
public:
 explicit WaveReader(const std::string& path);
 std::uint32_t rate() const noexcept {return rate_;}
 std::uint64_t frames() const noexcept {return frames_;}
 void read(std::uint64_t start,Stereo* dst,std::size_t count);
private:
 std::ifstream stream_;
 std::uint64_t dataOffset_=0,frames_=0;
 std::uint32_t rate_=0;
 std::uint16_t format_=0,channels_=0,bits_=0,blockAlign_=0;
};
class WaveWriter {
public:
 WaveWriter(const std::string& path,std::uint32_t sampleRate);
 ~WaveWriter();
 void append(const Stereo* data,std::size_t count);
 void finish();
private:
 std::ofstream stream_;
 std::uint64_t frames_=0;
 std::uint32_t rate_;
 bool finished_=false;
};
}
