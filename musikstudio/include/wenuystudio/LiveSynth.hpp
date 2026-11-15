// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "InstrumentBank.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace wenuystudio {
// Audio-thread-owned, fixed-size polyphonic synthesizer. No heap or locks.
class LiveSynth {
public:
 static constexpr std::size_t maxVoices=16;
 bool noteOn(int key,int instrument,float velocity) noexcept {
  if(key<0||key>127||!instrumentById(instrument)||
     !std::isfinite(velocity)||velocity<=0.f||velocity>1.f)return false;
  for(auto& v:voices_)if(v.used&&v.key==key){
   if(v.held)return true;
   reset(v,key,instrument,velocity);return true;
  }
  for(auto& v:voices_)if(!v.used){reset(v,key,instrument,velocity);return true;}
  Voice* victim=&voices_[0];
  for(auto& v:voices_)if((!v.held&&victim->held)||
      (v.held==victim->held&&v.order<victim->order))victim=&v;
  reset(*victim,key,instrument,velocity);
  return true;
 }
 void noteOff(int key) noexcept {
  if(key<0||key>127)return;
  for(auto& v:voices_)if(v.used&&v.key==key&&v.held){
   v.held=false;v.releaseAge=0;
  }
 }
 void allNotesOff() noexcept{
  for(auto& v:voices_)if(v.used&&v.held){v.held=false;v.releaseAge=0;}
 }
 std::size_t voiceCount()const noexcept {
  std::size_t n=0;for(const auto& v:voices_)if(v.used)++n;return n;
 }
 bool held(int key)const noexcept {
  for(const auto& v:voices_)if(v.used&&v.held&&v.key==key)return true;
  return false;
 }
 void render(float* stereo,std::size_t frames,std::uint32_t rate) noexcept {
  if(!stereo||rate<8000||rate>192000)return;
  constexpr double tau=6.2831853071795864769;
  const auto releaseFrames=std::max<std::uint32_t>(1,rate/8);
  for(std::size_t i=0;i<frames;++i){
   float sum=0;
   for(auto& v:voices_){
    if(!v.used)continue;
    const double t=double(v.age)/rate;
    if(t>=19999.){v.used=false;continue;}
    const float envelope=v.held?1.f:
     std::max(0.f,1.f-float(v.releaseAge)/releaseFrames);
    sum+=instrumentSample(v.instrument,v.phase,v.hz,t,20000.,rate)*
         (.14f*v.velocity*envelope);
    ++v.age;
    v.phase+=tau*v.hz/rate;
    if(v.phase>=tau)v.phase=std::fmod(v.phase,tau);
    if(!v.held&&++v.releaseAge>=releaseFrames)v.used=false;
   }
   const float sample=std::clamp(sum,-.98f,.98f);
   stereo[2*i]=sample;stereo[2*i+1]=sample;
  }
 }
private:
 struct Voice {
  bool used=false,held=false;int key=0,instrument=0;float velocity=0;
  double hz=0,phase=0;std::uint64_t age=0,order=0;
  std::uint32_t releaseAge=0;
 };
 void reset(Voice& v,int key,int instrument,float velocity)noexcept{
  v={true,true,key,instrument,velocity,
     440.*std::pow(2.,(key-69.)/12.),0,0,++serial_,0};
 }
 std::array<Voice,maxVoices> voices_{};
 std::uint64_t serial_=0;
};
} // namespace wenuystudio
