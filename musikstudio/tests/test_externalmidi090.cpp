// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/MidiInput.hpp"
#include "wenuystudio/MidiInputKeys.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
using namespace wenuystudio;

int main(){
 assert(!midiNoteMessage(true,-1,64,0));
 assert(!midiNoteMessage(true,128,64,0));
 assert(!midiNoteMessage(true,60,-1,0));
 assert(!midiNoteMessage(true,60,128,0));
 assert(!midiNoteMessage(true,60,100,-1));
 assert(!midiNoteMessage(true,60,100,16));
 for(int note=0;note<128;++note){
  auto on=midiNoteMessage(true,note,127,0);
  assert(on&&on->kind==MidiInputKind::NoteOn&&on->key==note);
  assert(std::abs(on->velocity-1.f)<.00001);
  auto zero=midiNoteMessage(true,note,0,1);
  assert(zero&&zero->kind==MidiInputKind::NoteOff&&zero->key==note);
  auto off=midiNoteMessage(false,note,64,15);
  assert(off&&off->kind==MidiInputKind::NoteOff&&off->channel==15);
 }
 auto quiet=midiNoteMessage(true,60,1,2);
 assert(quiet&&quiet->kind==MidiInputKind::NoteOn&&quiet->velocity>0.f);
 assert(!midiControlMessage(7,100,0)); // unrelated controller is ignored
 assert(!midiControlMessage(64,-1,0));
 assert(!midiControlMessage(64,128,0));
 const auto up=midiControlMessage(64,63,2);
 const auto down=midiControlMessage(64,64,2);
 assert(up&&up->kind==MidiInputKind::Sustain&&up->value==63&&up->channel==2);
 assert(down&&down->kind==MidiInputKind::Sustain&&down->value==64);
 assert(!midiControlMessage(123,0,-1));
 assert(!midiControlMessage(123,0,16));
 for(int channel=0;channel<16;++channel){
  const auto stop=midiControlMessage(123,0,channel);
  const auto panic=midiControlMessage(120,0,channel);
  assert(stop&&stop->kind==MidiInputKind::AllNotesOff&&stop->channel==channel);
  assert(panic&&panic->kind==MidiInputKind::AllNotesOff&&panic->channel==channel);
 }
 // Fixed-memory channel state: overlapping channels, CC64 and channel panic.
 MidiInputKeys held;
 int ons=0,offs=0;
 float lastVelocity=0.f;
 const auto emit=[&](int key,bool on,float vel){
  assert(key==60||key==64);
  if(on){++ons;lastVelocity=vel;}
  else{++offs;assert(vel==0.f);}
 };
 held.noteOn(0,60,.7f,emit);
 held.noteOn(1,60,.9f,emit);
 held.noteOn(1,60,.1f,emit); // repeated Note On does not leak counts
 assert(ons==1&&offs==0&&held.sounding(60));
 held.noteOff(0,60,emit);
 assert(offs==0&&held.sounding(60)); // channel 1 still holds C4
 held.sustain(1,true,emit);
 held.noteOff(1,60,emit);
 assert(offs==0&&held.sounding(60)); // CC64 delays Note Off
 held.sustain(1,false,emit);
 assert(offs==1&&!held.sounding(60));
 held.noteOff(1,60,emit);
 assert(offs==1);
 held.sustain(2,true,emit);
 held.noteOn(2,64,.5f,emit);
 held.noteOff(2,64,emit);
 assert(held.sounding(64)&&ons==2);
 held.noteOn(3,64,.4f,emit);
 held.allNotesOff(2,emit); // panic must not touch a different channel
 assert(held.sounding(64)&&offs==1);
 held.allNotesOff(3,emit);
 assert(!held.sounding(64)&&offs==2);
 held.noteOn(-1,60,.8f,emit);
 held.noteOn(16,60,.8f,emit);
 assert(!held.sounding(60));
 held.noteOn(0,60,.8f,emit);
 assert(lastVelocity==.8f&&held.sounding(60));
 held.reset(emit); // USB disconnect releases held note and pedal
 assert(!held.sounding(60)&&offs==3);
 held.reset(emit);
 assert(offs==3);
 MidiInput input;
#ifdef WENUYSTUDIO_TEST_ALSA
 assert(input.supported());
#else
 assert(!input.supported());
#endif
 assert(!input.connected());
 // An invalid source must never open ALSA or connect to any device.
 std::string error;
 const auto nop=[](const MidiInputMessage&){};
 assert(!input.connect({-1,0,"invalid"},nop,error));
 assert(!error.empty()&&!input.connected());
 error.clear();
 assert(!input.connect({1,0,"invalid"}, {},error));
 assert(!error.empty()&&!input.connected());
 input.disconnect();
 assert(!input.connected());
 // No audio/MIDI device is required for this headless unit test.
 std::cout<<"PASS: USB MIDI normalization, sustain CC64, channel panic, invalid sources\n";
}
