// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/LiveSynth.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace wenuystudio;
int main(){
 LiveSynth synth;
 assert(synth.noteOn(60,0,.8f)&&synth.noteOn(64,0,.8f)&&synth.noteOn(67,0,.8f));
 assert(synth.voiceCount()==3&&synth.held(60)&&synth.held(64)&&synth.held(67));
 std::array<float,512> pcm{};double energy=0;
 for(int i=0;i<12;++i){
  synth.render(pcm.data(),256,48000);
  for(float v:pcm){assert(std::isfinite(v)&&std::abs(v)<=.98f);energy+=std::abs(v);}
 }
 assert(energy>2.);
 synth.noteOff(64);
 assert(!synth.held(64)&&synth.held(60)&&synth.held(67));
 for(int i=0;i<26;++i)synth.render(pcm.data(),256,48000);
 assert(synth.voiceCount()==2&&synth.held(60)&&synth.held(67));
 assert(synth.noteOn(64,5,.7f)&&synth.noteOn(64,5,.7f)&&synth.voiceCount()==3);
 synth.allNotesOff();
 for(int i=0;i<26;++i)synth.render(pcm.data(),256,48000);
 assert(synth.voiceCount()==0);
 assert(!synth.noteOn(-1,0,.8f)&&!synth.noteOn(128,0,.8f));
 assert(!synth.noteOn(60,16,.8f)&&!synth.noteOn(60,0,-1.f));
 for(int n=40;n<70;++n)assert(synth.noteOn(n,n%16,.75f));
 assert(synth.voiceCount()==LiveSynth::maxVoices);
 synth.allNotesOff();
 for(int i=0;i<26;++i)synth.render(pcm.data(),256,48000);
 assert(synth.voiceCount()==0);
 std::cout<<"PASS: chord, independent release, bounded voices and panic\n";
}
