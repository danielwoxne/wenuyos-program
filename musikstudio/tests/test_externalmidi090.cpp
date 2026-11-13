// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/MidiInput.hpp"
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
 assert(!midiControlMessage(64,0)); // sustain pedal not silently translated to a note
 assert(!midiControlMessage(123,-1));
 assert(!midiControlMessage(123,16));
 for(int channel=0;channel<16;++channel){
  const auto stop=midiControlMessage(123,channel);
  const auto panic=midiControlMessage(120,channel);
  assert(stop&&stop->kind==MidiInputKind::AllNotesOff&&stop->channel==channel);
  assert(panic&&panic->kind==MidiInputKind::AllNotesOff&&panic->channel==channel);
 }
 MidiInput input;
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
 std::cout<<"PASS: USB MIDI normalization, zero-velocity off, CC panic, invalid sources\n";
}
