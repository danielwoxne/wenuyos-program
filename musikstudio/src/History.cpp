// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/History.hpp"

#include <utility>

namespace wenuystudio {

History::History(std::size_t maxBytes,std::size_t maxSteps):budget_(maxBytes),steps_(maxSteps){}

std::size_t History::weight(const Project& p) noexcept {

 std::size_t n=sizeof(Project);

 for(const auto& t:p.tracks){

  n+=sizeof(Track)+t.name.size();

  n+=t.notes.size()*sizeof(MidiNote);

  for(const auto& c:t.clips)n+=sizeof(AudioClip)+c.filename.size();

 }

 return n;

}

std::size_t History::memoryBytes() const noexcept {

 std::size_t n=0;

 for(const auto& p:undo_)n+=weight(p);

 for(const auto& p:redo_)n+=weight(p);

 return n;

}

void History::trim(){

 while((undo_.size()+redo_.size()>steps_ || memoryBytes()>budget_) && (!undo_.empty()||!redo_.empty())) {

  if(!undo_.empty())undo_.pop_front();else redo_.pop_front();

 }

}

void History::checkpoint(const Project& p){

 redo_.clear();

 if(steps_==0 || weight(p)>budget_){undo_.clear();return;}

 undo_.push_back(p);trim();

}

bool History::undo(Project& p){

 if(undo_.empty())return false;

 redo_.push_back(std::move(p));p=std::move(undo_.back());undo_.pop_back();trim();return true;

}

bool History::redo(Project& p){

 if(redo_.empty())return false;

 undo_.push_back(std::move(p));p=std::move(redo_.back());redo_.pop_back();trim();return true;

}

void History::clear() noexcept {undo_.clear();redo_.clear();}

}
