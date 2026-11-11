// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Engine.hpp"

#include "wenuystudio/ProjectIO.hpp"

#include "wenuystudio/MidiFile.hpp"

#include "wenuystudio/Sequencer.hpp"

#include "wenuystudio/Arrangement.hpp"

#include "wenuystudio/Wave.hpp"

#include <filesystem>

#include <iostream>

#include <stdexcept>

namespace ws=wenuystudio;

static ws::Project demo(){

 ws::Project p;p.durationSeconds=12;p.bpm=110;p.tracks.resize(4);

 p.tracks[0].name="Melody";p.tracks[1].name="Harmony";p.tracks[2].name="Bass";p.tracks[3].name="Audio";

 const int keys[]={60,64,67,72,69,67,64,60};

 for(int i=0;i<8;i++){p.tracks[0].notes.push_back({keys[i],double(i)*.545,.46,.75f});}

 for(int i=0;i<4;i++){p.tracks[1].notes.push_back({i%2?53:48,double(i)*2.18,1.8,.5f});p.tracks[2].notes.push_back({i%2?41:36,double(i)*2.18,1.4,.65f});}

 p.tracks[0].gain=.8f;p.tracks[1].gain=.5f;p.tracks[2].gain=.6f;p.tracks[2].pan=-.2f;

 p.loop={false,0,4.36};return p;

}

static void help(){std::cout<<"Wenuy Studio 0.7.0 - standalone C++ workstation\n"

 "Usage:\n"

 "  wenuy-studio-cli demo PROJECT.wstudio\n"

 "  wenuy-studio-cli info PROJECT.wstudio\n"

 "  wenuy-studio-cli render PROJECT.wstudio OUTPUT.wav\n"

 "  wenuy-studio-cli midi-import INPUT.mid OUTPUT.wstudio\n"

 "  wenuy-studio-cli midi-export INPUT.wstudio OUTPUT.mid\n"

 "  wenuy-studio-cli drums INPUT.wstudio OUTPUT.wstudio DRUMS.wav [bars]\n"

 "  wenuy-studio-cli quantize INPUT.wstudio OUTPUT.wstudio TRACK SUBDIVISIONS\n";

}

int main(int argc,char** argv){

 try{

  if(argc<2){help();return 0;}

  std::string cmd=argv[1];

  if((cmd=="help"||cmd=="--help")&&argc==2){help();return 0;}

  if(cmd=="demo"&&argc==3){ws::saveProject(demo(),argv[2]);std::cout<<"Created demo project\n";return 0;}

  if(cmd=="info"&&argc==3){auto p=ws::loadProject(argv[2]);

   std::cout<<"Studio 0.8: "<<p.tracks.size()<<" tracks, "<<p.durationSeconds<<" sec, "<<p.bpm<<" BPM\n";

   for(auto& t:p.tracks)std::cout<<"  "<<t.name<<": "<<t.notes.size()<<" notes, "<<t.clips.size()<<" audio clips\n";

   return 0;}

  if(cmd=="render"&&argc==4){ws::render(ws::loadProject(argv[2]),argv[3]);std::cout<<"WAV written: "<<argv[3]<<"\n";return 0;}

  if(cmd=="midi-import"&&argc==4){ws::saveProject(ws::importMidi(argv[2]),argv[3]);return 0;}

  if(cmd=="midi-export"&&argc==4){ws::exportMidi(ws::loadProject(argv[2]),argv[3]);return 0;}

  if(cmd=="quantize"&&argc==6){auto p=ws::loadProject(argv[2]);

   auto n=ws::quantizeTrack(p,std::stoul(argv[4]),{static_cast<unsigned>(std::stoul(argv[5])),0.f});

   ws::saveProject(p,argv[3]);std::cout<<"Quantized "<<n<<" notes\n";return 0;}

  if(cmd=="drums"&&(argc==5||argc==6)){

   auto p=ws::loadProject(argv[2]);ws::DrumPattern pat;pat.bars=argc==6?std::stoul(argv[5]):4;

   ws::generateDrumLoop(argv[4],p.bpm,p.sampleRate,pat);

   ws::Track t;t.name="Drum Machine";t.clips.push_back({argv[4],0,0,pat.bars*4*60/p.bpm,1});

   p.durationSeconds=std::max(p.durationSeconds,t.clips[0].durationSeconds);p.tracks.push_back(std::move(t));

   ws::saveProject(p,argv[3]);return 0;}

  help();return 2;

 }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<'\n';return 1;}

}
