// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/AudioIO.hpp"

#include "wenuystudio/History.hpp"

#include "wenuystudio/Waveform.hpp"

#include "wenuystudio/Wave.hpp"

#include <cmath>

#include <filesystem>

#include <iostream>

#include <stdexcept>

#include <vector>

namespace fs=std::filesystem;

static void check(bool okay,const char* msg){if(!okay)throw std::runtime_error(msg);}

int main(){

 const fs::path wav=fs::temp_directory_path()/"wenuystudio-03-wave-test.wav";

 try{

  wenuystudio::Project p;

  wenuystudio::Track t;t.name="A";p.tracks.push_back(t);

  wenuystudio::History h(1024*1024,3);

  h.checkpoint(p);p.tracks[0].gain=0.4f;

  h.checkpoint(p);p.tracks[0].mute=true;

  check(h.undo(p)&&!p.tracks[0].mute,"Undo mute");

  check(std::abs(p.tracks[0].gain-0.4f)<.0001,"Undo retained gain");

  check(h.undo(p)&&std::abs(p.tracks[0].gain-1.0f)<.0001,"Undo gain");

  check(h.redo(p)&&std::abs(p.tracks[0].gain-0.4f)<.0001,"Redo gain");

  h.checkpoint(p);p.tracks[0].name="B";

  check(!h.canRedo(),"New edits must invalidate redo");

  wenuystudio::History tiny(1,2);tiny.checkpoint(p);check(!tiny.canUndo(),"Bounded history");

  {

   wenuystudio::WaveWriter writer(wav.string(),48000);

   std::vector<wenuystudio::Stereo> frames(4800);

   for(std::size_t i=0;i<frames.size();++i){frames[i]={static_cast<float>(std::sin(2*3.141592653589793*i/48.0)),0};}

   writer.append(frames.data(),frames.size());writer.finish();

  }

  const auto peak=wenuystudio::waveform(wav.string(),240);

  check(peak.size()==240,"Peak count");

  bool loud=false;

  for(const auto& b:peak){check(std::isfinite(b.min)&&std::isfinite(b.max)&&b.min<=b.max,"Invalid waveform peak");if(b.max>.25f)loud=true;}

  check(loud,"Missing waveform signal");

  bool rejected=false;

  try{(void)wenuystudio::waveform(wav.string(),5000);}catch(const std::invalid_argument&){rejected=true;}

  check(rejected,"Unbounded waveform accepted");

  fs::remove(wav);

  std::cout<<"History and bounded waveform tests passed\n";

  return 0;

 }catch(const std::exception& e){std::error_code ec;fs::remove(wav,ec);std::cerr<<e.what()<<'\n';return 1;}

}
