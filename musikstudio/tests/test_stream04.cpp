// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/BlockMixer.hpp"

#include "wenuystudio/AudioIO.hpp"

#include "wenuystudio/ProjectIO.hpp"

#include "wenuystudio/Waveform.hpp"

#include <array>

#include <chrono>

#include <cmath>

#include <cstdlib>

#include <filesystem>

#include <fstream>

#include <iostream>

#include <stdexcept>

#include <string>

#include <thread>

#include <unistd.h>

namespace fs=std::filesystem;

using namespace wenuystudio;

static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}

int main(){

 char tmp[]="/tmp/wenuystudio04-XXXXXX";

 char* unique=mkdtemp(tmp);

 if(!unique){std::cerr<<"Cannot create test folder\n";return 2;}

 const fs::path dir=unique;

 try{

  const auto source=dir/"transient.wav";

  {

   WaveWriter writer(source.string(),48000);

   std::array<Stereo,4800> buffer{};

   buffer[1234].r=0.95f;

   writer.append(buffer.data(),buffer.size());writer.finish();

  }

  auto accurate=waveformAccurate(source.string(),12);

  check(accurate.size()==12,"Exact peak array size");

  check(accurate[3].max>.90f,"The isolated right-channel transient must not be dropped");

  for(const auto& peak:accurate)check(peak.min<=peak.max,"Bad min/max in accurate peaks");

  bool caught=false;

  try{(void)waveformAccurate(source.string(),4097);}catch(const std::invalid_argument&){caught=true;}

  check(caught,"Accurate peak cap must be enforced");

  Project project;

  project.durationSeconds=.25;

  project.masterGain=.5f;

  Track first;first.name="Piano";first.notes.push_back({69,0,.20f,1.0f});

  Track second;second.name="Synth";second.notes.push_back({69,0,.20f,1.0f});

  project.tracks={first,second};

  BlockMixer together(project);

  std::array<Stereo,512> buffer{};

  BlockMeter both;

  check(together.process(2048,buffer.data(),buffer.size(),&both)==buffer.size(),"Block read size");

  check(both.left>.01f && both.right>.01f,"Mix is silent");

  const float doubled=both.left;

  project.tracks[0].solo=true;

  BlockMixer onlyFirst(project);

  BlockMeter single;

  onlyFirst.process(2048,buffer.data(),buffer.size(),&single);

  check(std::abs(single.left*2.0f-doubled)<.001f,"Solo ignored or master gain wrong");

  project.tracks[0].mute=true; // muted solo must not silence all other tracks

  BlockMixer muteSolo(project);

  BlockMeter muted;

  muteSolo.process(2048,buffer.data(),buffer.size(),&muted);

  check(std::abs(muted.left-single.left)<.001f,"Muted solo suppressed other tracks");

  project.tracks[0].mute=false;

  project.tracks[0].solo=false;

  project.tracks[1].pan=-1.0f;

  BlockMixer panned(project);

  BlockMeter pans;

  panned.process(2048,buffer.data(),buffer.size(),&pans);

  check(pans.left>pans.right+.01f,"Pan not applied");

  const auto path=dir/"v3.wstudio";

  project.tracks[0].solo=true;

  saveProject(project,path.string());

  const auto loaded=loadProject(path.string());

  check(loaded.tracks[0].solo,"Solo must roundtrip through project v3");

  check(std::abs(loaded.masterGain-.5f)<1e-6f,"Master gain must roundtrip");

  const auto legacy=dir/"v2.wstudio";

  {

   std::ofstream old(legacy);

   old<<"WENUYSTUDIO 2\nRATE 48000\nDURATION 1\nBPM 120\nTRACK \"Old\" 1 0 0\nENDTRACK\nEND\n";

  }

  const auto p2=loadProject(legacy.string());

  check(!p2.tracks.front().solo && p2.masterGain==1.0f,"Legacy project v2 compatibility failed");

  const auto output=dir/"render.wav";

  render(project,output.string());

  WaveReader rendered(output.string());

  check(rendered.frames()==12000,"Export sample count");

  BlockMixer reference(project);

  const auto count=reference.process(2048,buffer.data(),buffer.size());

  std::array<Stereo,512> fromFile{};

  rendered.read(2048,fromFile.data(),count);

  for(std::size_t i=0;i<count;++i){

   check(std::abs(buffer[i].l-fromFile[i].l)<.00006f,"Offline and block audio disagree (left)");

   check(std::abs(buffer[i].r-fromFile[i].r)<.00006f,"Offline and block audio disagree (right)");

  }

  Project pPlay=project;

  pPlay.durationSeconds=.05;

  Playback playback;

  playback.start(std::move(pPlay));

  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);

  while(playback.active() && std::chrono::steady_clock::now()<deadline)

   std::this_thread::sleep_for(std::chrono::milliseconds(5));

  playback.stop();

  check(playback.error().empty(),"Simulated live block playback failed");

  check(playback.positionSeconds()>=.049,"Live block playback did not reach end");

  fs::remove_all(dir);

  std::cout<<"0.4 streamed playback, mixer controls, v2/v3, export parity and exact peaks passed\n";

  return 0;

 }catch(const std::exception& e){fs::remove_all(dir);std::cerr<<e.what()<<'\n';return 1;}

}
