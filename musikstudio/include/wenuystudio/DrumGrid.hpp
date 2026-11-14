// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Sequencer.hpp"
#include <cstddef>
#include <string>
namespace wenuystudio {
// The grid is transient UI state; a generated drum track consists entirely
// of ordinary MIDI notes in .wstudio v5 and can be edited/undone/exported.
constexpr int drumInstrumentId=16;
constexpr int drumKickKey=36, drumSnareKey=38, drumHiHatKey=42;
struct DrumInsertResult {
 std::size_t trackIndex=0;
 std::size_t insertedNotes=0;
 double endSeconds=0;
};
// Validates everything before mutating the project (strong error guarantee).
// Starts at an absolute playhead time, not necessarily at bar zero.
DrumInsertResult appendDrumTrack(Project& project,const DrumPattern& pattern,
                                 double fromSeconds,const std::string& trackName);
}
