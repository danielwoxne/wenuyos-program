// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/ProjectIO.hpp"

#include <cmath>

#include <filesystem>

#include <fstream>

#include <stdexcept>

#include <string>

#include <iostream>

namespace fs=std::filesystem;

static void require(bool c,const char* why){if(!c)throw std::runtime_error(why);}

int main(){

 const fs::path dir=fs::temp_directory_path()/"wenuystudio-v02-test";

 fs::create_directories(dir);

 const fs::path path=dir/"my song.wstudio";

 try{

  wenuystudio::Project p;p.durationSeconds=7.25;p.bpm=96.5;

  wenuystudio::Track t;t.name="Sång \"One\"";t.pan=-0.3f;

  t.clips.push_back({(dir/"take 1.wav").string(),1.0,0.2,0.5,0.6f});

  t.notes.push_back({48,2.0,0.25,0.5f});p.tracks.push_back(t);

  wenuystudio::saveProject(p,path.string());

  auto loaded=wenuystudio::loadProject(path.string());

  require(loaded.tracks.size()==1,"Track count");

  require(loaded.tracks[0].name==t.name,"UTF8 or quotes");

  require(loaded.tracks[0].notes.size()==1,"Notes count");

  require(loaded.tracks[0].clips[0].filename==(dir/"take 1.wav").string(),"Relative audio path");

  require(std::fabs(loaded.bpm-96.5)<0.00001,"Tempo roundtrip");

  p.tracks[0].name="Ny version";wenuystudio::saveProject(p,path.string());

  require(fs::exists(path.string()+".bak"),"Missing previous-save backup");

  bool rejected=false;

  {std::ofstream out(path,std::ios::trunc);out<<"WENUYSTUDIO 999\n";}

  try{(void)wenuystudio::loadProject(path.string());}catch(const std::runtime_error&){rejected=true;}

  require(rejected,"Unsupported project version accepted");

  rejected=false;

  {std::ofstream out(path,std::ios::trunc);out<<"WENUYSTUDIO 2\nRATE 48000\nDURATION 5\nBPM 120\nTRACK \"X\" 1 0 0\nCLIP \"a.wav\" 0 0 3 1\nENDTRACK\nEND\nGARBAGE\n";}

  try{(void)wenuystudio::loadProject(path.string());}catch(const std::runtime_error&){rejected=true;}

  require(rejected,"Trailing content accepted");

  fs::remove_all(dir);std::cout<<"Project save/load tests passed\n";return 0;

 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';fs::remove_all(dir);return 1;}

}
