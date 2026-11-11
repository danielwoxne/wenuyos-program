// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Wave.hpp"
#include "TrackFX.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
namespace wenuystudio {
struct AudioClip {std::string filename;double startSeconds=0,sourceOffsetSeconds=0,durationSeconds=0;float gain=1;};
struct MidiNote {int key=60;double atSeconds=0,durationSeconds=.5;float velocity=.8f;};
struct Track {
 std::string name;float gain=1,pan=0;bool mute=false,solo=false;
 TrackEffects effects{};
 int instrument=0; // 0 sine, 1 triangle, 2 organ, 3 warm lead
 std::vector<AudioClip> clips;
 std::vector<MidiNote> notes;
};
struct LoopRegion {bool enabled=false;double startSeconds=0,endSeconds=0;};
struct Project {
 std::uint32_t sampleRate=48000;
 double durationSeconds=12,bpm=120;
 float masterGain=1;
 LoopRegion loop{};
 std::vector<Track> tracks;
};
void render(const Project&,const std::string&,const std::atomic<bool>* keepRunning=nullptr);
}
