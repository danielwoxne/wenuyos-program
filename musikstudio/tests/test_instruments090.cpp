// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/InstrumentBank.hpp"
#include "wenuystudio/BlockMixer.hpp"
#include "wenuystudio/ProjectIO.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace wenuystudio;
int main() {
 assert(instrumentCount()==16);
 assert(!instrumentById(-1)&&!instrumentById(16));
 assert(instrumentById(0)&&std::string(instrumentById(0)->english)=="Sine");
 assert(instrumentById(1)&&std::string(instrumentById(1)->english)=="Triangle");
 assert(instrumentById(2)&&std::string(instrumentById(2)->english)=="Organ");
 assert(instrumentById(3)&&std::string(instrumentById(3)->english)=="Warm lead");
 const auto dir=std::filesystem::temp_directory_path()/"wenuystudio-instrument-090-tests";
 std::filesystem::create_directories(dir);
 try {
  int different=0;double firstEnergy=-1;
  for(int id=0;id<static_cast<int>(instrumentCount());++id) {
   const auto* preset=instrumentById(id);
   assert(preset&&preset->id==id&&*preset->swedish&&*preset->familySwedish);
   Project p;p.sampleRate=8000;p.durationSeconds=1;
   Track tr;tr.name="Test";tr.instrument=id;
   tr.notes.push_back({60,.10,.50,.8f});
   p.tracks.push_back(tr);
   const auto projectPath=(dir/(std::to_string(id)+".wstudio")).string();
   saveProject(p,projectPath);
   const auto loaded=loadProject(projectPath);
   assert(loaded.tracks.size()==1&&loaded.tracks[0].instrument==id);
   BlockMixer mixer(loaded);
   Stereo out[512]{};
   assert(mixer.process(800,out,512)==512);
   double energy=0;
   for(const auto& s:out) {
    assert(std::isfinite(s.l)&&std::isfinite(s.r));
    assert(std::abs(s.l)<=.3f&&std::abs(s.r)<=.3f);
    energy+=std::abs(s.l);
   }
   assert(energy>.25);
   if(firstEnergy>0&&std::abs(energy-firstEnergy)>.01)++different;
   if(firstEnergy<0)firstEnergy=energy;
  }
  assert(different>=8);
  Project invalid;invalid.tracks.emplace_back();invalid.tracks[0].instrument=16;
  bool rejected=false;try{saveProject(invalid,(dir/"bad.wstudio").string());}catch(const std::exception&){rejected=true;}
  assert(rejected);
  invalid.tracks[0].instrument=-1;
  rejected=false;try{saveProject(invalid,(dir/"bad.wstudio").string());}catch(const std::exception&){rejected=true;}
  assert(rejected);
  assert(instrumentSample(0,1.0,440,0,1,48000)==0);
  assert(instrumentSample(4,1.0,440,1,1,48000)==0);
  assert(instrumentSample(10,1.0,440,-1,1,48000)==0);
  std::filesystem::remove_all(dir);
  std::cout<<"PASS: 16 presets, audio energy, project v5, old IDs, invalid IDs\n";
 }catch(...){std::filesystem::remove_all(dir);throw;}
}
