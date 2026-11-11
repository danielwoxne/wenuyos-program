// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/EditModel.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
namespace wenuystudio {
EditHistory::EditHistory(std::size_t bytes,std::size_t steps):byteLimit_(bytes),stepLimit_(steps){}
std::size_t EditHistory::estimatedBytes(const Project& p) noexcept {
 std::size_t bytes=sizeof(Project);
 for(const auto& t:p.tracks){
  bytes+=sizeof(Track)+t.name.size()+t.clips.size()*sizeof(AudioClip)+t.notes.size()*sizeof(MidiNote);
  for(const auto& c:t.clips)bytes+=c.filename.size();
 }
 return bytes;
}
void EditHistory::trim(){
 auto total=[this](){
  std::size_t used=0;
  for(const auto& p:past_)used+=estimatedBytes(p);
  for(const auto& p:future_)used+=estimatedBytes(p);
  return used;
 };
 while((past_.size()+future_.size()>stepLimit_ || total()>byteLimit_) && (!past_.empty()||!future_.empty())){
  if(!past_.empty())past_.pop_front();else future_.pop_front();
 }
}
bool EditHistory::checkpoint(const Project& current){
 if(stepLimit_==0 || estimatedBytes(current)>byteLimit_/2)return false;
 past_.push_back(current);future_.clear();trim();return !past_.empty();
}
bool EditHistory::undo(Project& current){
 if(past_.empty())return false;
 Project replacement=past_.back();future_.push_back(current);past_.pop_back();current=std::move(replacement);trim();return true;
}
bool EditHistory::redo(Project& current){
 if(future_.empty())return false;
 Project replacement=future_.back();past_.push_back(current);future_.pop_back();current=std::move(replacement);trim();return true;
}
void EditHistory::clear()noexcept{past_.clear();future_.clear();}
bool quantizeNotes(Project& p,std::size_t track,int divisions){
 if(track>=p.tracks.size()||divisions<1||divisions>64||!std::isfinite(p.bpm)||p.bpm<20||p.bpm>400)return false;
 const double grid=60.0/(p.bpm*divisions);
 for(auto& n:p.tracks[track].notes)if(!std::isfinite(n.atSeconds)||n.atSeconds<0||n.atSeconds>20000)return false;
 bool changed=false;
 for(auto& n:p.tracks[track].notes){
  const auto at=std::clamp(std::round(n.atSeconds/grid)*grid,0.0,p.durationSeconds);
  if(at!=n.atSeconds){n.atSeconds=at;changed=true;}
 }
 return changed;
}
bool transposeNotes(Project& p,std::size_t track,int semitones){
 if(track>=p.tracks.size()||semitones==0||semitones< -127||semitones>127)return false;
 for(const auto& n:p.tracks[track].notes)if(n.key+semitones<0||n.key+semitones>127)return false;
 for(auto& n:p.tracks[track].notes)n.key+=semitones;
 return !p.tracks[track].notes.empty();
}
bool moveClipTo(Project& p,std::size_t track,std::size_t clip,double seconds){
 if(track>=p.tracks.size()||clip>=p.tracks[track].clips.size()||!std::isfinite(seconds)||seconds<0||seconds>p.durationSeconds)return false;
 auto& c=p.tracks[track].clips[clip];if(c.startSeconds==seconds)return false;
 c.startSeconds=seconds;return true;
}
bool duplicateClipTo(Project& p,std::size_t track,std::size_t clip,double seconds){
 if(track>=p.tracks.size()||clip>=p.tracks[track].clips.size()||!std::isfinite(seconds)||seconds<0||seconds>p.durationSeconds)return false;
 std::size_t count=0;for(const auto& t:p.tracks)count+=t.clips.size();if(count>=4096)return false;
 auto copy=p.tracks[track].clips[clip];copy.startSeconds=seconds;p.tracks[track].clips.push_back(std::move(copy));return true;
}
}
