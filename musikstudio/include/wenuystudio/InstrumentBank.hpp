// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

namespace wenuystudio {
// Fixed, stable IDs: 0–3 are the original 0.8.1 sounds.
// Presets are synthetic approximations, not recordings or sampled instruments.
struct InstrumentPreset {
 int id;
 const char* english;
 const char* swedish;
 const char* familyEnglish;
 const char* familySwedish;
};
std::size_t instrumentCount() noexcept;
const InstrumentPreset* instrumentById(int id) noexcept;
// Pure oscillator: no allocations, file I/O, callbacks or locks in the audio thread.
// Includes the note envelope and returns an amplitude-limited sample.
float instrumentSample(int id,double phase,double fundamentalHz,
                       double noteTime,double noteDuration,std::uint32_t sampleRate) noexcept;
}
