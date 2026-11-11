// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Wave.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace wenuystudio {
// Bounded, single-producer/single-consumer (SPSC) stereo ring buffer.
// Construct and destroy outside the PortAudio callback. Producer and consumer
// may be on different threads; only one thread may call each method at a time.
// Buffer access is allocation-free, lock-free and never performs disk I/O.
class AudioBuffer final {
public:
 explicit AudioBuffer(std::size_t capacityFrames);
 AudioBuffer(const AudioBuffer&)=delete;
 AudioBuffer& operator=(const AudioBuffer&)=delete;
 std::size_t capacity() const noexcept {return frames_.size();}
 std::size_t available() const noexcept;
 std::size_t freeFrames() const noexcept;
 std::size_t push(const Stereo* source,std::size_t count) noexcept;
 // Consumes available samples into interleaved float32 output, and zero fills
 // any remainder. Returns actual non-silent-buffer frames consumed (which may
 // themselves be digitally silent). peakL and peakR cover only consumed frames.
 std::size_t popInterleaved(float* target,std::size_t count,
                            float& peakL,float& peakR) noexcept;
private:
 std::vector<Stereo> frames_;
 std::size_t mask_;
 alignas(64) std::atomic<std::uint64_t> write_{0};
 alignas(64) std::atomic<std::uint64_t> read_{0};
};
}
