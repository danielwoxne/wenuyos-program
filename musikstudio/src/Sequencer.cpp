// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Sequencer.hpp"

#include "wenuystudio/Wave.hpp"

#include <algorithm>

#include <array>

#include <cmath>

#include <filesystem>

#include <unistd.h>

#include <limits>

#include <stdexcept>

#include <vector>

namespace wenuystudio {

namespace {

constexpr double pi=3.14159265358979323846;

void require(bool b, const char* message){if(!b) throw std::invalid_argument(message);}

void gridValid(EditGrid g){require(g.subdivisions>=1&&g.subdivisions<=32&&

  std::isfinite(g.swing)&&g.swing>=0.0f&&g.swing<=0.45f,"Invalid grid or swing");}

void tempoValid(double bpm){require(std::isfinite(bpm)&&bpm>=20&&bpm<=400,"Invalid BPM");}

void projectValid(const Project& p,std::size_t track){require(track<p.tracks.size(),"Track not found");

  tempoValid(p.bpm);require(std::isfinite(p.durationSeconds)&&p.durationSeconds>0,"Invalid project duration");}

void noteValid(const MidiNote& n){require(n.key>=0&&n.key<=127&&std::isfinite(n.atSeconds)&&

 n.atSeconds>=0&&std::isfinite(n.durationSeconds)&&n.durationSeconds>0&&

 std::isfinite(n.velocity)&&n.velocity>=0&&n.velocity<=1,"Invalid MIDI note");}

void order(Track& track){std::stable_sort(track.notes.begin(),track.notes.end(),

 [](const MidiNote&a,const MidiNote&b){return a.atSeconds<b.atSeconds ||

 (a.atSeconds==b.atSeconds&&a.key<b.key);});}

struct DrumVoice {double started=-1000.0; double weight=1.0;};

float soft(float x){return x/(1.0f+std::abs(x));}

}

double beatSeconds(double bpm){tempoValid(bpm);return 60.0/bpm;}

double snapTime(double sec,double bpm,EditGrid g){

 tempoValid(bpm);gridValid(g);require(std::isfinite(sec)&&sec>=0&&sec<=20000,"Invalid time");

 const double unit=beatSeconds(bpm)/g.subdivisions;

 const double step=std::floor(sec/unit+0.5);

 // Swing delays odd-grid steps but never beyond the next step.

 return std::max(0.0,step*unit+(static_cast<std::uint64_t>(step)&1U ? g.swing*unit:0.0));

}

std::string pitchName(int key){

 require(key>=0&&key<=127,"Invalid MIDI key");

 constexpr const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

 return std::string(names[key%12])+std::to_string(key/12-1);

}

EditResult insertNote(Project& p,std::size_t track,MidiNote note,EditGrid grid){

 projectValid(p,track);noteValid(note);gridValid(grid);

 note.atSeconds=snapTime(note.atSeconds,p.bpm,grid);

 if(note.atSeconds>=p.durationSeconds)return {};

 note.durationSeconds=std::min(note.durationSeconds,p.durationSeconds-note.atSeconds);

 const auto& existing=p.tracks[track].notes;

 for(std::size_t i=0;i<existing.size();++i){

  if(existing[i].key==note.key&&std::abs(existing[i].atSeconds-note.atSeconds)<1e-7)return {false,i};

 }

 if(existing.size()>=50000)return {};

 p.tracks[track].notes.push_back(note);order(p.tracks[track]);

 for(std::size_t i=0;i<p.tracks[track].notes.size();++i)

  if(p.tracks[track].notes[i].key==note.key&&

    std::abs(p.tracks[track].notes[i].atSeconds-note.atSeconds)<1e-7)return {true,i};

 return {};

}

bool removeNote(Project& p,std::size_t track,std::size_t index){

 projectValid(p,track);auto& notes=p.tracks[track].notes;

 if(index>=notes.size())return false;

 notes.erase(notes.begin()+static_cast<std::ptrdiff_t>(index));return true;

}

bool moveNote(Project& p,std::size_t track,std::size_t index,int key,double at,EditGrid grid){

 projectValid(p,track);auto& notes=p.tracks[track].notes;

 if(index>=notes.size())return false;

 require(key>=0&&key<=127&&std::isfinite(at)&&at>=0&&at<p.durationSeconds,"Invalid note position");

 const double snapped=snapTime(at,p.bpm,grid);

 if(snapped>=p.durationSeconds)return false;

 for(std::size_t i=0;i<notes.size();++i)if(i!=index&&notes[i].key==key&&

   std::abs(notes[i].atSeconds-snapped)<1e-7)return false;

 bool changed=notes[index].key!=key||std::abs(notes[index].atSeconds-snapped)>1e-7;

 notes[index].key=key;notes[index].atSeconds=snapped;

 notes[index].durationSeconds=std::min(notes[index].durationSeconds,p.durationSeconds-snapped);

 order(p.tracks[track]);return changed;

}

std::size_t quantizeTrack(Project& p,std::size_t track,EditGrid grid){

 projectValid(p,track);gridValid(grid);std::size_t count=0;

 for(auto& n:p.tracks[track].notes){

  noteValid(n);double pos=snapTime(n.atSeconds,p.bpm,grid);

  if(pos>=p.durationSeconds)pos=std::max(0.0,p.durationSeconds-beatSeconds(p.bpm)/grid.subdivisions);

  if(std::abs(pos-n.atSeconds)>1e-7){n.atSeconds=pos;++count;}

  n.durationSeconds=std::min(n.durationSeconds,p.durationSeconds-n.atSeconds);

 }

 order(p.tracks[track]);return count;

}

std::size_t duplicateBars(Project& p,std::size_t track,unsigned bar,unsigned bars,unsigned dest){

 projectValid(p,track);require(bars>=1&&bars<=128,"Invalid number of bars");

 const double len=4.0*beatSeconds(p.bpm);

 const double begin=bar*len,end=(bar+bars)*len,delta=(static_cast<double>(dest)-bar)*len;

 std::vector<MidiNote> copy;

 for(const auto& n:p.tracks[track].notes){

  if(n.atSeconds>=begin&&n.atSeconds<end&&n.atSeconds+delta>=0&&n.atSeconds+delta<p.durationSeconds){

   auto c=n;c.atSeconds+=delta;c.durationSeconds=std::min(c.durationSeconds,p.durationSeconds-c.atSeconds);

   copy.push_back(c);

  }

 }

 if(copy.size()+p.tracks[track].notes.size()>50000)throw std::length_error("Too many notes");

 const auto added=copy.size();auto& notes=p.tracks[track].notes;

 notes.insert(notes.end(),copy.begin(),copy.end());order(p.tracks[track]);return added;

}

void generateDrumLoop(const std::string& path,double bpm,std::uint32_t rate,const DrumPattern& pattern){

 tempoValid(bpm);require(rate>=8000&&rate<=192000,"Invalid audio rate");

 require(pattern.bars>=1&&pattern.bars<=64&&std::isfinite(pattern.swing)&&

 pattern.swing>=0&&pattern.swing<=0.45f,"Invalid drum pattern");

 namespace fs=std::filesystem;

 require(!fs::exists(fs::u8path(path)),"Output WAV exists; choose another filename");

 const double quarter=beatSeconds(bpm);

 const double stepDuration=quarter/4.0;

 const double duration=quarter*4.0*pattern.bars;

 const auto total=static_cast<std::uint64_t>(std::llround(duration*rate));

 // Each drum hit is rendered from its onset without maintaining long-lived dynamic voices.

 // Percussion decay <= 250ms. At 20 BPM a hit cannot overlap an adjacent step.

 std::vector<std::pair<double,int>> hits;hits.reserve(pattern.bars*48);

 for(unsigned bar=0;bar<pattern.bars;++bar)for(int step=0;step<16;++step){

  const double at=(bar*16.0+step)*stepDuration + (step&1?pattern.swing*stepDuration:0);

  if(pattern.kick&(1u<<step))hits.emplace_back(at,0);

  if(pattern.snare&(1u<<step))hits.emplace_back(at,1);

  if(pattern.hihat&(1u<<step))hits.emplace_back(at,2);

 }

 std::array<Stereo,512> out{};

 // Make a unique sidecar; only atomically link it to the desired name after finish.

 // Thus a crash or competing writer cannot truncate somebody else's WAV.

 std::string templateName=path+".part-XXXXXX";

 std::vector<char> tmp(templateName.begin(),templateName.end());tmp.push_back('\0');

 const int fd=::mkstemp(tmp.data());

 if(fd<0)throw std::runtime_error("Cannot create temporary drum WAV");

 ::close(fd);

 const std::string temporary=tmp.data();

 try {

 // Deterministic per-sample noise: identical renders on all machines.

 std::uint32_t rng=0x3bd18b17u;float prevNoise=0;

 WaveWriter writer(temporary,rate);

 for(std::uint64_t pos=0;pos<total;){

  const std::size_t n=static_cast<std::size_t>(std::min<std::uint64_t>(out.size(),total-pos));

  const double earliest=double(pos)/rate-.24;

  const double latest=double(pos+n)/rate;

  const auto firstHit=std::lower_bound(hits.begin(),hits.end(),earliest,

      [](const auto& hit,double sec){return hit.first<sec;});

  const auto lastHit=std::upper_bound(firstHit,hits.end(),latest,

      [](double sec,const auto& hit){return sec<hit.first;});

  for(std::size_t i=0;i<n;++i){

   const double t=double(pos+i)/rate;

   rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;

   const float noise=(static_cast<float>(rng&0xffffu)/32767.5f)-1.0f;

   const float hp=noise-prevNoise;prevNoise=noise;

   double v=0;

   // 250ms look-back bounds the work to a few nearby steps even for long sessions.

   for(auto hit=firstHit;hit!=lastHit;++hit){

    const double dt=t-hit->first;

    if(dt<0||dt>.24)continue;

    if(hit->second==0){ // kick: decaying falling sinusoid

     const double phase=2*pi*(47*dt+100*(1-std::exp(-dt*33))/33);

     v+=0.9*std::exp(-dt*25)*std::sin(phase);

    }else if(hit->second==1){

     v+=0.32*std::exp(-dt*28)*hp+0.17*std::exp(-dt*40)*std::sin(2*pi*185*dt);

    }else{

     v+=0.14*std::exp(-dt*90)*hp;

    }

   }

   float sample=soft(static_cast<float>(v));out[i]={sample,sample};

  }

  writer.append(out.data(),n);pos+=n;

 }

 writer.finish();

 fs::create_hard_link(fs::u8path(temporary),fs::u8path(path));

 fs::remove(fs::u8path(temporary));

 }catch(...){std::error_code ec;fs::remove(fs::u8path(temporary),ec);throw;}

}

}
