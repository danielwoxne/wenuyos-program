// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/EditModel.hpp"
#include <cassert>
#include <cmath>
using namespace wenuystudio;
int main(){
 Project p;p.bpm=120;p.durationSeconds=12;Track t;t.name="Lead";
 t.notes={{60,.26,.4,.9f},{64,1.23,.4,.6f}};
 t.clips.push_back({"track.wav",0,0,1,1});p.tracks.push_back(t);
 EditHistory h(2*1024*1024,4);
 assert(h.checkpoint(p));
 assert(quantizeNotes(p,0,2));assert(std::abs(p.tracks[0].notes[0].atSeconds-.25)<1e-9);
 assert(h.undo(p));assert(std::abs(p.tracks[0].notes[0].atSeconds-.26)<1e-9);
 assert(h.redo(p));assert(std::abs(p.tracks[0].notes[0].atSeconds-.25)<1e-9);
 assert(h.checkpoint(p));assert(transposeNotes(p,0,5));assert(p.tracks[0].notes[0].key==65);
 assert(!transposeNotes(p,0,100));assert(p.tracks[0].notes[0].key==65);
 assert(moveClipTo(p,0,0,3));assert(!moveClipTo(p,0,0,-1));
 assert(duplicateClipTo(p,0,0,5));assert(p.tracks[0].clips.size()==2);
 assert(!duplicateClipTo(p,0,0,1e10));
 assert(h.undo(p));assert(p.tracks[0].notes[0].key==60);
 assert(h.checkpoint(p));assert(!h.redo(p));
 for(int i=0;i<30;++i){h.checkpoint(p);p.bpm=120+i;}
 assert(h.undoCount()<=4);h.clear();assert(!h.undo(p));
}
