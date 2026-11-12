// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
namespace wenuystudio {
// All times in the existing .wstudio v3 model remain seconds.
// A beat is a quarter note. Grid subdivisions are counted per beat.
struct EditGrid {
  unsigned subdivisions = 4; // 1/16 note at four subdivisions per quarter beat
  float swing = 0.0f;         // 0..0.45, every second subdivision is delayed
};
struct EditResult { bool changed=false; std::size_t index=0; };

double beatSeconds(double bpm);
double snapTime(double seconds, double bpm, EditGrid grid);
std::string pitchName(int midiKey);
EditResult insertNote(Project& project, std::size_t track, MidiNote note, EditGrid grid);
bool removeNote(Project& project, std::size_t track, std::size_t noteIndex);
bool moveNote(Project& project, std::size_t track, std::size_t noteIndex,
              int key, double startSeconds, EditGrid grid);
std::size_t quantizeTrack(Project& project, std::size_t track, EditGrid grid);
std::size_t duplicateBars(Project& project, std::size_t track, unsigned sourceBar,
                          unsigned bars, unsigned destinationBar);
struct DrumPattern {
  // 16 steps per bar. A 1 means that instrument plays at that step.
  std::uint16_t kick=0x1111;
  std::uint16_t snare=0x1010;
  std::uint16_t hihat=0x5555;
  unsigned bars=4;
  float swing=0.0f;
};
// Synthesize a real PCM16 WAV, using a bounded 512-frame scratch buffer.
// No samples or nonfree assets required; file must not exist already.
void generateDrumLoop(const std::string& wavPath, double bpm,
                      std::uint32_t rate, const DrumPattern& pattern);
}
