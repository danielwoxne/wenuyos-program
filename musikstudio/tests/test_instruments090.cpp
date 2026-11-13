// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/InstrumentBank.hpp"
#include "wenuystudio/BlockMixer.hpp"
#include "wenuystudio/ProjectIO.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
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
  // Exact backward-compatible waveforms for original ID 0-3 at 48 kHz.
  // Keep these checks even if the oscillator implementation is optimized.
  constexpr double pi=3.14159265358979323846;
  for(double phase: {0.37,1.1,2.3,4.9}) {
   const double s1=std::sin(phase),s2=std::sin(2*phase),s3=std::sin(3*phase);
   const std::array<double,4> legacy={{
    s1,(2/pi)*std::asin(s1),
    .7*s1+.25*s2+.15*s3,
    .65*s1+.20*std::sin(1.005*phase)+.12*s2
   }};
   for(int id=0;id<4;++id) {
    const float actual=instrumentSample(id,phase,220.0,.2,.6,48000);
    assert(std::abs(actual-static_cast<float>(legacy[static_cast<std::size_t>(id)]))<1e-6f);
   }
  }
  // Eight simultaneous tracks, four overlapping MIDI notes per track.
  // Offline playback exercises the same BlockMixer as PortAudio / WAV export.
  Project eight;
  eight.sampleRate=48000;eight.durationSeconds=2.5;
  for(int id=0;id<8;++id) {
   Track track;track.name="Stress "+std::to_string(id);
   track.instrument=id;track.gain=.18f;
   for(int n=0;n<4;++n)
    track.notes.push_back({48+id+n*3,.10+n*.22,1.15,.8f});
   eight.tracks.push_back(std::move(track));
  }
  const auto session=(dir/"eight-track.wstudio").string();
  saveProject(eight,session);
  const auto restored=loadProject(session);
  assert(restored.tracks.size()==8);
  for(std::size_t i=0;i<8;++i)
   assert(restored.tracks[i].instrument==static_cast<int>(i) &&
          restored.tracks[i].notes.size()==4);
  BlockMixer eightMixer(restored);
  assert(eightMixer.totalFrames()==120000);
  std::array<Stereo,512> frameBlock{};
  double energyTotal=0,peak=0;
  const auto begin=std::chrono::steady_clock::now();
  for(std::uint64_t at=0;at<eightMixer.totalFrames();at+=frameBlock.size()) {
   BlockMeter meter{};
   const auto count=eightMixer.process(at,frameBlock.data(),frameBlock.size(),&meter);
   assert(count>0&&count<=frameBlock.size());
   for(std::size_t n=0;n<count;++n) {
    assert(std::isfinite(frameBlock[n].l)&&std::isfinite(frameBlock[n].r));
    energyTotal+=std::abs(frameBlock[n].l)+std::abs(frameBlock[n].r);
    peak=std::max(peak,std::max(std::abs(static_cast<double>(frameBlock[n].l)),
                                std::abs(static_cast<double>(frameBlock[n].r))));
   }
   assert(std::isfinite(meter.left)&&std::isfinite(meter.right));
  }
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now()-begin).count();
  assert(energyTotal>100.0&&peak>0.01&&peak<2.0);
  std::cout<<"Eight-track offline mix 48 kHz, 32 notes: "<<elapsed
           <<" ms, peak="<<peak<<"\n";
  std::filesystem::remove_all(dir);
  std::cout<<"PASS: 16 presets, audio energy, project v5, old IDs, invalid IDs\n";
 }catch(...){std::filesystem::remove_all(dir);throw;}
}
