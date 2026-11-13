// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Sequencer.hpp"
#include <cmath>
#include <cstddef>
#include <optional>

namespace wenuystudio {
// Shared geometry for GTK painting, pointer handling and headless tests.
// One coordinate system avoids a clicked note landing on another pitch.
constexpr double pianoLeft=65.0;
constexpr double pianoTop=28.0;
constexpr double pianoPixelsPerSecond=50.0;
constexpr double pianoRowHeight=12.0;
constexpr int pianoHighestKey=84; // C6
constexpr int pianoLowestKey=36;  // C2

struct PianoPosition {
 int key;
 double seconds;
};

inline std::optional<PianoPosition> pianoPositionAt(double x,double y,double durationSeconds) {
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(durationSeconds)||
    durationSeconds<=0||x<pianoLeft||y<pianoTop||
    y>=pianoTop+(pianoHighestKey-pianoLowestKey+1)*pianoRowHeight)return std::nullopt;
 const double seconds=(x-pianoLeft)/pianoPixelsPerSecond;
 if(seconds<0||seconds>=durationSeconds)return std::nullopt;
 const int key=pianoHighestKey-static_cast<int>(std::floor((y-pianoTop)/pianoRowHeight));
 return PianoPosition{key,seconds};
}

// Reverse order mirrors painting: the most recently drawn overlapping note wins.
inline std::optional<std::size_t> pianoNoteAt(const Track& track,const PianoPosition& pos) {
 for(std::size_t i=track.notes.size();i>0;--i) {
  const auto& n=track.notes[i-1];
  if(n.key==pos.key&&std::isfinite(n.atSeconds)&&std::isfinite(n.durationSeconds)&&
     n.durationSeconds>0&&pos.seconds>=n.atSeconds&&
     pos.seconds<n.atSeconds+n.durationSeconds)return i-1;
 }
 return std::nullopt;
}

// Also detect a note starting in the same snapped cell when its length is short.
inline std::optional<std::size_t> pianoNoteAtGrid(
    const Track& track,const PianoPosition& pos,double bpm,EditGrid grid) {
 const double snapped=snapTime(pos.seconds,bpm,grid);
 for(std::size_t i=0;i<track.notes.size();++i) {
  const auto& n=track.notes[i];
  if(n.key==pos.key&&std::abs(n.atSeconds-snapped)<1e-7)return i;
 }
 return std::nullopt;
}
} // namespace wenuystudio
