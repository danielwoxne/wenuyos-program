// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/BlockMixer.hpp"
#include "wenuystudio/ProjectIO.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
using namespace wenuystudio;
int main(){
 Project p;p.sampleRate=48000;p.durationSeconds=2;p.masterGain=.65f;
 Track tr;tr.name="Stereo pan";tr.gain=.7f;tr.pan=-.85f;
 tr.effects.enabled=true;tr.effects.thresholdDb=-22.f;
 tr.effects.ratio=3.5f;tr.effects.lowDb=2.f;
 tr.instrument=0;tr.notes.push_back({60,.1,1.,.8f});
 p.tracks.push_back(tr);
 const auto filename=(std::filesystem::temp_directory_path()/
   "wenuystudio-mixer-090.wstudio").string();
 try{
  saveProject(p,filename);
  auto loaded=loadProject(filename);
  assert(loaded.tracks.size()==1);
  assert(std::abs(loaded.masterGain-.65f)<.001f);
  assert(std::abs(loaded.tracks[0].pan+.85f)<.001f);
  assert(std::abs(loaded.tracks[0].effects.thresholdDb+22.f)<.001f);
  assert(std::abs(loaded.tracks[0].effects.ratio-3.5f)<.001f);
  BlockMixer mixer(loaded);
  std::array<Stereo,512> out{};
  assert(mixer.process(4800,out.data(),out.size())==out.size());
  double left=0,right=0;
  for(const auto& s:out){
   assert(std::isfinite(s.l)&&std::isfinite(s.r));
   left+=std::abs(s.l);right+=std::abs(s.r);
  }
  assert(left>right*2&&right>0);
  std::filesystem::remove(filename);
 }catch(...){std::error_code error;std::filesystem::remove(filename,error);throw;}
 std::cout<<"PASS: mixer project master, pan and FX persist and render stereo\n";
}
