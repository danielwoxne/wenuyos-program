// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
namespace wenuystudio {
// A reusable, seekable, bounded-memory block mixer.
// Build/prepare (including opening WAV files and allocations) on a worker.
// process() performs no dynamic allocations and never opens audio files.
// WAV reads are blocking: this is worker-thread streaming, NOT hard real-time DSP.
struct BlockMeter {float left=0.0f, right=0.0f;};
class BlockMixer final {
public:
 explicit BlockMixer(Project project);
 ~BlockMixer();
 BlockMixer(const BlockMixer&)=delete;
 BlockMixer& operator=(const BlockMixer&)=delete;
 BlockMixer(BlockMixer&&) noexcept;
 BlockMixer& operator=(BlockMixer&&) noexcept;
 std::uint32_t sampleRate() const noexcept;
 std::uint64_t totalFrames() const noexcept;
 // Returns number of actual project frames; the rest of dst stays silent.
 // count must be <= 1024, and dst must be a valid buffer.
 std::size_t process(std::uint64_t start,Stereo* dst,std::size_t count,BlockMeter* meter=nullptr);
private:
 struct Impl;
 std::unique_ptr<Impl> impl_;
};
}
