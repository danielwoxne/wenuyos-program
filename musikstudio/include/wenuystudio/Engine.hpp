// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Wave.hpp"
#include <cstdint>
#include <atomic>
#include <string>
#include <memory>
#include <cstddef>
#include <vector>
namespace wenuystudio {
struct AudioClip {
    std::string filename;
    double startSeconds=0.0;
    double sourceOffsetSeconds=0.0;
    double durationSeconds=0.0; // 0 means until end of file
    float gain=1.0f;
};
struct MidiNote {
    int key=60;
    double atSeconds=0.0;
    double durationSeconds=0.5;
    float velocity=0.8f;
};
struct Track {
    std::string name;
    float gain=1.0f;
    float pan=0.0f; // -1 left, 1 right
    bool mute=false;
    bool solo=false; // hear only unmuted solo tracks when any solo is enabled
    std::vector<AudioClip> clips;
    std::vector<MidiNote> notes;
};
struct Project {
    std::uint32_t sampleRate=48000;
    double durationSeconds=5.0;
    double bpm=120.0;
    float masterGain=1.0f; // linear fader: 0..4
    std::vector<Track> tracks;
};
// Bounded-block offline reference engine. Future realtime engine reuses its immutable model.
void render(const Project& project, const std::string& outputFile, const std::atomic<bool>* keepRunning=nullptr);
}
