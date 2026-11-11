// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <cstddef>
#include <deque>
namespace wenuystudio {
class EditHistory final {
public:
 explicit EditHistory(std::size_t byteLimit=12*1024*1024,std::size_t stepLimit=32);
 bool checkpoint(const Project& current);
 bool undo(Project& current);
 bool redo(Project& current);
 void clear() noexcept;
 std::size_t undoCount() const noexcept{return past_.size();}
 std::size_t redoCount() const noexcept{return future_.size();}
private:
 std::size_t byteLimit_,stepLimit_;
 std::deque<Project> past_,future_;
 static std::size_t estimatedBytes(const Project&) noexcept;
 void trim();
};
bool quantizeNotes(Project&,std::size_t track,int subdivisionsPerBeat);
bool transposeNotes(Project&,std::size_t track,int semitones);
bool moveClipTo(Project&,std::size_t track,std::size_t clip,double atSeconds);
bool duplicateClipTo(Project&,std::size_t track,std::size_t clip,double atSeconds);
}
