// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Wave.hpp"
#include <cstddef>
#include <cstdint>
namespace wenuystudio {
// A simple, intentionally small CPU-only effects chain, per track.
// Unity bypass is bit-preserving; filter and dynamics never allocate in process().
struct TrackEffects {
 bool enabled=false;
 float lowDb=0.f, midDb=0.f, highDb=0.f; // -18..+18 dB
 float thresholdDb=-18.f;                 // -48..0 dB
 float ratio=2.f;                         // 1..12
};
bool validTrackEffects(const TrackEffects& fx) noexcept;
class TrackFX final {
public:
 void configure(const TrackEffects& fx,std::uint32_t rate);
 void reset() noexcept;
 void process(Stereo* frames,std::size_t count) noexcept;
private:
 TrackEffects fx_{};
 float lowCoeff_=0, highCoeff_=0;
 float lowGain_=1, midGain_=1, highGain_=1;
 float attack_=0, release_=0, threshold_=1, exponent_=0;
 float lowL_=0, lowR_=0, highLpL_=0, highLpR_=0, env_=0;
 float gain_=1.f,targetGain_=1.f;
 unsigned decimation_=0;
 bool enabled_=false;
};
}
