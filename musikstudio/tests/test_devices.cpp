// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/AudioIO.hpp"

#include "wenuystudio/Wave.hpp"

#include <chrono>

#include <cstdlib>

#include <cmath>

#include <filesystem>

#include <iostream>

#include <stdexcept>

#include <thread>

namespace fs=std::filesystem;

static void check(bool c,const char* text){if(!c)throw std::runtime_error(text);}

int main(){

 const auto file=fs::temp_directory_path()/"wenuystudio-03-fake-record.wav";

 try{

  // No physical audio device required: the PortAudio ABI comes from fake library.

  const auto report=wenuystudio::queryAudioDevices();

  check(report.devices.size()==2,"Wrong number of simulated audio devices");

  check(report.defaultInput==0&&report.defaultOutput==1,"Default devices incorrect");

  check(report.devices[0].name=="Test Microphone"&&report.devices[0].defaultInput,"Input device wrong");

  check(report.devices[1].name=="Test Speakers"&&report.devices[1].defaultOutput,"Output device wrong");

  check(report.devices[1].hostApi=="Wenuy Test Backend","Host API missing");

  check(report.devices[1].defaultSampleRate==48000,"Default device rate wrong");

  std::error_code ec;fs::remove(file,ec);fs::remove(file.string()+".part",ec);

  wenuystudio::Project p;p.durationSeconds=.3;

  wenuystudio::Track t;t.name="Synth";t.notes.push_back({60,0,.2,.6f});p.tracks.push_back(t);

  wenuystudio::Playback player;player.start(p,0.1);

  for(int i=0;i<100&&player.active();++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));

  check(!player.active(),"Fake device playback timed out");

  player.stop();check(player.error().empty(),"Fake device playback error");

  check(player.positionSeconds()>.1,"Playback cursor did not move");

  wenuystudio::Recording rec;rec.start(file.string(),48000);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));rec.stop();

  check(rec.error().empty(),"Fake device recording error");

  check(rec.frames()>0,"Fake microphone was not recorded");

  wenuystudio::WaveReader sound(file.string());

  check(sound.frames()==rec.frames(),"Record file duration differs");

  check(sound.rate()==48000,"Recorded wrong sample rate");

  check(!fs::exists(file.string()+".part"),"Recording temp file leaked");

  fs::remove(file);

  // API initialization failure must be surfaced without crash or leaked state.

  setenv("WENUY_FAKE_AUDIO_FAIL","1",1);

  bool gotError=false;

  try{(void)wenuystudio::queryAudioDevices();}

  catch(const std::exception&){gotError=true;}

  unsetenv("WENUY_FAKE_AUDIO_FAIL");

  check(gotError,"Initialization failure was not reported");

  std::cout<<"Simulated playback and recording passed\n";return 0;

 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::error_code ec;fs::remove(file,ec);return 1;}

}
