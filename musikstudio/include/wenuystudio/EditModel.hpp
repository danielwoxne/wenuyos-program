// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <cstddef>
#include <deque>
namespace wenuystudio {
// Memory-bounded, session-local undo history. Never records audio sample data.
class EditHistory final {
public:
 explicit EditHistory(std::size_t byteLimit=12*1024*1024,std::size_t stepLimit=32);
 bool checkpoint(const Project& current);
 bool undo(Project& current);
 bool redo(Project& current);
 void clear() noexcept;
 std::size_t undoCount() const noexcept {return past_.size();}
 std::size_t redoCount() const noexcept {return future_.size();}
private:
 std::size_t byteLimit_,stepLimit_;
 std::deque<Project> past_,future_;
 static std::size_t estimatedBytes(const Project&) noexcept;
 void trim();
};
// Non-destructive editing helpers. Return false on invalid arguments; no partial edits.
bool quantizeNotes(Project& project,std::size_t track,int subdivisionsPerBeat);
bool transposeNotes(Project& project,std::size_t track,int semitones);
bool moveClipTo(Project& project,std::size_t track,std::size_t clip,double startSeconds);
bool duplicateClipTo(Project& project,std::size_t track,std::size_t clip,double startSeconds);
}
