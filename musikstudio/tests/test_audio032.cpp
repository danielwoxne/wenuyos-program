// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/AudioIO.hpp"

#include "wenuystudio/Wave.hpp"

#include <chrono>

#include <cstdlib>

#include <filesystem>

#include <fstream>

#include <iostream>

#include <stdexcept>

#include <string>

#include <thread>

namespace fs=std::filesystem;

static void require(bool okay,const char* reason){if(!okay)throw std::runtime_error(reason);}

int main(){

 const fs::path path=fs::temp_directory_path()/"wenuy-studio-032-record.wav";

 std::error_code ec;fs::remove(path,ec);

 try{

  // Probe is explicit, unlike passive devices/doctor listing.

  auto p=wenuystudio::probeDefaultAudioDevices();

  require(p.inputOpened&&p.outputOpened,"Both fake audio streams must open");

  setenv("WENUY_FAKE_NO_INPUT","1",1);

  p=wenuystudio::probeDefaultAudioDevices();

  require(!p.inputOpened&&p.outputOpened&&!p.inputError.empty(),"Missing microphone must be reported");

  unsetenv("WENUY_FAKE_NO_INPUT");

  setenv("WENUY_FAKE_OPEN_FAIL","1",1);

  p=wenuystudio::probeDefaultAudioDevices();

  require(!p.inputOpened&&!p.outputOpened&&!p.inputError.empty()&&!p.outputError.empty(),"Open error must not crash");

  unsetenv("WENUY_FAKE_OPEN_FAIL");

  // An existing WAV must never be overwritten, even by accidental record.

  {std::ofstream orig(path);orig<<"KEEP ORIGINAL";}

  bool rejected=false;

  try{wenuystudio::Recording r;r.start(path.string(),48000);r.stop();}

  catch(const std::runtime_error&){rejected=true;}

  require(rejected,"Existing destination not rejected");

  {std::ifstream orig(path);std::string s;std::getline(orig,s);require(s=="KEEP ORIGINAL","Original WAV was overwritten");}

  fs::remove(path);

  // A valid recording is published and partials are cleaned up.

  wenuystudio::Recording recording;

  recording.start(path.string(),48000);

  std::this_thread::sleep_for(std::chrono::milliseconds(35));

  recording.stop();

  require(recording.error().empty(),"Recording error");

  require(recording.frames()>0,"No recorded frames");

  wenuystudio::WaveReader wav(path.string());

  require(wav.frames()==recording.frames(),"Published WAV frame mismatch");

  require(wav.rate()==48000,"Published WAV sample rate mismatch");

  const auto parent=path.parent_path();

  const auto prefix=path.filename().string()+".part-";

  for(const auto& entry:fs::directory_iterator(parent)){

   const auto name=entry.path().filename().string();

   require(name.compare(0,prefix.size(),prefix)!=0,"Leftover partial WAV");

  }

  fs::remove(path);

  std::cout<<"0.3.2 active device probes and recording safety passed\n";

  return 0;

 }catch(const std::exception& e){fs::remove(path,ec);std::cerr<<e.what()<<'\n';return 1;}

}
