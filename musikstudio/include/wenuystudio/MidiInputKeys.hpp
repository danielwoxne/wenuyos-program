// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace wenuystudio {
// Collapses the 16 local MIDI channels to a pitch-aware, bounded live keyboard.
// Physical key states and CC64 sustain are independent per channel: releasing
// one channel can never release a note still sounding on another channel.
// No allocation, timers, background threads, or global state.
class MidiInputKeys {
public:
 template<class Sink>
 void noteOn(int channel,int key,float velocity,Sink&& sink){
  if(!valid(channel,key)||pressed_[channel][key])return;
  const bool alreadySounding=pressed_[channel][key]||latched_[channel][key];
  pressed_[channel][key]=true;
  latched_[channel][key]=false;
  if(!alreadySounding && ++sounding_[key]==1)sink(key,true,velocity);
 }
 template<class Sink>
 void noteOff(int channel,int key,Sink&& sink){
  if(!valid(channel,key)||!pressed_[channel][key])return;
  pressed_[channel][key]=false;
  if(pedal_[channel])latched_[channel][key]=true;
  else if(!latched_[channel][key])stopKey(key,sink);
 }
 template<class Sink>
 void sustain(int channel,bool down,Sink&& sink){
  if(channel<0||channel>=16||pedal_[channel]==down)return;
  pedal_[channel]=down;
  if(!down){
   for(int key=0;key<128;++key){
    if(!latched_[channel][key])continue;
    latched_[channel][key]=false;
    if(!pressed_[channel][key])stopKey(key,sink);
   }
  }
 }
 template<class Sink>
 void allNotesOff(int channel,Sink&& sink){
  if(channel<0||channel>=16)return;
  pedal_[channel]=false;
  for(int key=0;key<128;++key){
   if(!pressed_[channel][key]&&!latched_[channel][key])continue;
   pressed_[channel][key]=false;
   latched_[channel][key]=false;
   stopKey(key,sink);
  }
 }
 template<class Sink>
 void reset(Sink&& sink){
  for(int channel=0;channel<16;++channel)allNotesOff(channel,sink);
 }
 bool sounding(int key)const noexcept{
  return key>=0&&key<128&&sounding_[key]>0;
 }
private:
 static bool valid(int channel,int key)noexcept{
  return channel>=0&&channel<16&&key>=0&&key<128;
 }
 template<class Sink>
 void stopKey(int key,Sink&& sink){
  if(sounding_[key]&&--sounding_[key]==0)sink(key,false,0.f);
 }
 std::array<std::array<bool,128>,16> pressed_{};
 std::array<std::array<bool,128>,16> latched_{};
 std::array<bool,16> pedal_{};
 std::array<std::uint8_t,128> sounding_{};
};
} // namespace wenuystudio
