// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace wenuystudio {
// Small, allocation-free state for notes held by a physical or on-screen
// keyboard. Only noteOff / finish may append to a capped MIDI take.
class MidiTake {
public:
 static constexpr std::size_t maxTakeNotes=4096;
 bool begin(double projectAtSeconds,std::int64_t monotonicMicroseconds) noexcept {
  if(active_||!std::isfinite(projectAtSeconds)||projectAtSeconds<0||
     projectAtSeconds>=20000||monotonicMicroseconds<0)return false;
  notes_.clear();held_.fill({});
  offset_=projectAtSeconds;begin_=monotonicMicroseconds;active_=true;
  return true;
 }
 bool recording()const noexcept{return active_;}
 std::size_t recorded()const noexcept{return notes_.size();}
 bool noteOn(int key,std::int64_t now,float velocity=.8f) noexcept {
  if(!active_||key<0||key>127||now<begin_||!std::isfinite(velocity)||
     velocity<=0.f||velocity>1.f||held_[static_cast<std::size_t>(key)].active)return false;
  auto& held=held_[static_cast<std::size_t>(key)];
  held={true,now,velocity};
  return true;
 }
 bool noteOff(int key,std::int64_t now) {
  if(!active_||key<0||key>127)return false;
  auto& held=held_[static_cast<std::size_t>(key)];
  if(!held.active)return false;
  const bool room=notes_.size()<maxTakeNotes;
  if(room){
   const double at=offset_+double(held.at-begin_)*.000001;
   const double seconds=double(std::max(now,held.at)-held.at)*.000001;
   if(at<20000){
    const double duration=std::min(std::max(.05,seconds),20000-at);
    if(duration>0)notes_.push_back({key,at,duration,held.velocity});
   }
  }
  held.active=false;
  return room;
 }
 std::vector<MidiNote> finish(std::int64_t now) {
  if(!active_)return {};
  for(int key=0;key<128;++key)
   if(held_[static_cast<std::size_t>(key)].active)noteOff(key,now);
  active_=false;held_.fill({});
  return std::exchange(notes_,{});
 }
private:
 struct Held {bool active=false;std::int64_t at=0;float velocity=.8f;};
 std::array<Held,128> held_{};
 std::vector<MidiNote> notes_;
 double offset_=0;
 std::int64_t begin_=0;
 bool active_=false;
};
}
