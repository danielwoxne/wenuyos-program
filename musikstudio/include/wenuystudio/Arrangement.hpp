// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <cstddef>
namespace wenuystudio {
// Non-destructive editing: WAV source files are never modified or deleted.
// A duration=0 clip means "to end of file" and must be given an explicit
// length by the user before split/trim; editing never scans media on UI thread.
bool moveAudioClip(Project& project,std::size_t track,std::size_t clip,double newStart);
bool splitAudioClip(Project& project,std::size_t track,std::size_t clip,double atSeconds);
bool trimAudioClip(Project& project,std::size_t track,std::size_t clip,double beginSeconds,double endSeconds);
bool duplicateAudioClip(Project& project,std::size_t track,std::size_t clip,double newStart);
bool setLoopRegion(Project& project,bool enabled,double start,double end);
}
