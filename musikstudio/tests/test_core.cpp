// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Engine.hpp"

#include "wenuystudio/ProjectIO.hpp"

#include "wenuystudio/BlockMixer.hpp"

#include "wenuystudio/MidiFile.hpp"

#include "wenuystudio/Sequencer.hpp"

#include "wenuystudio/Arrangement.hpp"

#include "wenuystudio/Wave.hpp"

#ifdef NDEBUG

#undef NDEBUG

#endif

#include <cassert>

#include <cmath>

#include <filesystem>

#include <fstream>

#include <iostream>

namespace fs=std::filesystem;namespace ws=wenuystudio;

int main(){

 fs::path root=fs::temp_directory_path()/"wenuystudio-070-tests";

 fs::create_directories(root);

 try{

  ws::Project p;p.durationSeconds=2;p.tracks.push_back(ws::Track{});

  p.tracks[0].name="Test";p.tracks[0].notes.push_back({69,.1,.5,.7f});

  p.tracks[0].effects.enabled=true;p.tracks[0].effects.lowDb=3;p.tracks[0].instrument=2;

  p.loop={true,.1,1.5};

  ws::saveProject(p,(root/"project.wstudio").string());

  auto b=ws::loadProject((root/"project.wstudio").string());

  assert(b.tracks.size()==1&&b.tracks[0].effects.lowDb==3&&b.loop.enabled&&b.tracks[0].instrument==2);

  ws::BlockMixer mix(b);

  ws::Stereo frames[1024]{};ws::BlockMeter meter{};

  assert(mix.process(5500,frames,1024,&meter)==1024);

  assert(meter.left>.001f&&meter.right>.001f);

  ws::render(b,(root/"audio.wav").string());

  ws::WaveReader wav((root/"audio.wav").string());

  assert(wav.rate()==48000&&wav.frames()==96000);

  assert(ws::insertNote(b,0,{64,.76,.2,.9f},{4,0}).changed);

  assert(ws::quantizeTrack(b,0,{4,0})<=2);

  ws::exportMidi(b,(root/"notes.mid").string());

  auto imported=ws::importMidi((root/"notes.mid").string());

  assert(!imported.tracks.empty());

  auto p2=ws::Project{};p2.durationSeconds=4;p2.tracks.push_back(ws::Track{});

  p2.tracks[0].clips.push_back({(root/"audio.wav").string(),0,0,2,1});

  assert(ws::splitAudioClip(p2,0,0,1));

  assert(p2.tracks[0].clips.size()==2);

  assert(ws::setLoopRegion(p2,true,.2,1.8));

  {std::ofstream old(root/"old.wstudio");

   old<<"WENUYSTUDIO 2\nRATE 48000\nDURATION 2\nBPM 120\nTRACK \"Legacy\" 1 0 0\nNOTE 60 0 .5 .8\nENDTRACK\nEND\n";}

  auto old=ws::loadProject((root/"old.wstudio").string());assert(old.tracks.size()==1);

  {std::ofstream old3(root/"old3.wstudio");

   old3<<"WENUYSTUDIO 3\nRATE 48000\nDURATION 2\nBPM 120\nMASTER 1\nTRACK \"Legacy3\" 1 0 0 0\nNOTE 60 0 .5 .8\nENDTRACK\nEND\n";}

  assert(ws::loadProject((root/"old3.wstudio").string()).tracks.size()==1);

  fs::remove_all(root);

  std::cout<<"PASS: project v2/v3/v5, instrument, EQ, audio, MIDI, arrangement, export\n";

 }catch(...){fs::remove_all(root);throw;}

}
