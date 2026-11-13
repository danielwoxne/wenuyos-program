// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/PianoInput.hpp"
#include "wenuystudio/EditModel.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace wenuystudio;

int main() {
 constexpr EditGrid grid{4,0};
 Project project;
 project.bpm=120;
 project.durationSeconds=12;
 project.tracks.emplace_back();
 auto& track=project.tracks[0];

 // Pixel-to-pitch mapping must match the rows displayed by the GTK canvas.
 const auto point=pianoPositionAt(pianoLeft+.24*pianoPixelsPerSecond,
   pianoTop+(pianoHighestKey-60)*pianoRowHeight+6,project.durationSeconds);
 assert(point && point->key==60 && std::abs(point->seconds-.24)<1e-10);
 assert(pianoPositionAt(pianoLeft,pianoTop,12)->key==84);
 assert(pianoPositionAt(pianoLeft,pianoTop+48*pianoRowHeight+11.9,12)->key==36);
 assert(!pianoPositionAt(pianoLeft-1,pianoTop,12));
 assert(!pianoPositionAt(pianoLeft,pianoTop-1,12));
 assert(!pianoPositionAt(pianoLeft,pianoTop+49*pianoRowHeight,12));
 assert(!pianoPositionAt(pianoLeft+12*pianoPixelsPerSecond,pianoTop,12));
 assert(!pianoPositionAt(std::numeric_limits<double>::quiet_NaN(),pianoTop,12));
 assert(!pianoPositionAt(pianoLeft,pianoTop,std::numeric_limits<double>::infinity()));

 EditHistory history;
 assert(history.checkpoint(project));
 const auto added=insertNote(project,0,{point->key,point->seconds,.125,.8f},grid);
 assert(added.changed && track.notes.size()==1);
 assert(std::abs(track.notes[0].atSeconds-.25)<1e-10);
 assert(!pianoNoteAt(track,*point)); // clicked before the snapped onset
 const auto onNote=pianoPositionAt(pianoLeft+.28*pianoPixelsPerSecond,
   pianoTop+(pianoHighestKey-60)*pianoRowHeight+6,12);
 assert(onNote && pianoNoteAt(track,*onNote)==added.index);
 assert(pianoNoteAtGrid(track,*point,project.bpm,grid)==added.index);
 assert(!insertNote(project,0,{60,.24,.125,.8f},grid).changed);
 assert(track.notes.size()==1);

 // Directional note movement uses the existing validated sequencer and undo.
 assert(history.checkpoint(project));
 assert(moveNote(project,0,added.index,61,.39,grid));
 assert(track.notes[0].key==61 && std::abs(track.notes[0].atSeconds-.375)<1e-10);
 assert(history.undo(project));
 assert(project.tracks[0].notes[0].key==60);
 assert(history.redo(project));
 assert(project.tracks[0].notes[0].key==61);

 // Right-click/Delete removes only the hit note, and the removal can be undone.
 assert(history.checkpoint(project));
 assert(removeNote(project,0,0));
 assert(project.tracks[0].notes.empty());
 assert(history.undo(project));
 assert(project.tracks[0].notes.size()==1);

 std::cout<<"PASS: piano coordinates, grid insert, select, move, delete, undo/redo\n";
}
