// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/AudioBuffer.hpp"

#include "wenuystudio/AudioIO.hpp"

#include <array>

#include <atomic>

#include <chrono>

#include <cmath>

#include <iostream>

#include <stdexcept>

#include <thread>

#include <vector>

using namespace wenuystudio;

static void check(bool value,const char* msg){if(!value)throw std::runtime_error(msg);}

int main(){

 try{

  for(const auto p:{BufferPreset::Low,BufferPreset::Balanced,BufferPreset::Safe}){

   const auto settings=audioBufferSettings(p);

   check(settings.callbackFrames<=settings.prefillFrames,"prefill < callback");

   check(settings.prefillFrames<settings.capacityFrames,"prefill >= capacity");

   check((settings.capacityFrames & (settings.capacityFrames-1))==0,"not power of two");

  }

  AudioBuffer ring(512);

  std::array<Stereo,600> source{};

  for(std::size_t i=0;i<source.size();++i){source[i].l=float(i+1);source[i].r=-float(i+1);}

  check(ring.push(source.data(),400)==400,"first push");

  check(ring.push(source.data()+400,200)==112,"full buffer must not overwrite");

  check(ring.available()==512&&ring.freeFrames()==0,"full buffer accounting");

  std::array<float,600> first{};

  float l=0,r=0;

  check(ring.popInterleaved(first.data(),300,l,r)==300,"first pop");

  check(first[0]==1&&first[1]==-1&&first[598]==300,"first pop order");

  check(ring.push(source.data()+512,88)==88,"wrap-around push");

  check(ring.available()==300,"wrap-around count");

  std::array<float,700> second{};

  check(ring.popInterleaved(second.data(),350,l,r)==300,"underflow partial count");

  check(second[0]==301&&second[1]==-301,"wrap-around start");

  check(second[2*211]==512&&second[2*212]==513,"wrap-around boundary");

  check(second[2*299]==600&&second[2*300]==0&&second[2*349]==0,"underrun must zero-fill");

  check(ring.available()==0,"ring empty");

  std::atomic<bool> produced{false};

  AudioBuffer concurrent(1024);

  std::thread producer([&]{

   std::array<Stereo,128> b{};

   for(int block=0;block<100;++block){

    for(int i=0;i<128;++i){const auto n=float(block*128+i);b[i]={n,-n};}

    std::size_t sent=0;

    while(sent<128){

     sent+=concurrent.push(b.data()+sent,128-sent);

     if(sent<128)std::this_thread::yield();

    }

   }

   produced.store(true);

  });

  std::array<float,256> sink{};

  int got=0;

  bool bad=false;

  while(got<12800){

   const auto n=concurrent.popInterleaved(sink.data(),128,l,r);

   for(std::size_t i=0;i<n;++i){

    if(sink[2*i]!=float(got)||sink[2*i+1]!=-float(got))bad=true;

    ++got;

   }

   if(n==0)std::this_thread::yield();

  }

  producer.join();

  check(produced.load()&&!bad&&concurrent.available()==0,"concurrent ring ordering");

  // Callback playback simulation: uses the same entry point as the GUI.

  Project project;project.durationSeconds=.10;

  Track t;t.notes.push_back({69,0,.09,0.8f});project.tracks.push_back(t);

  for(auto p:{BufferPreset::Low,BufferPreset::Balanced,BufferPreset::Safe}){

   Playback player;

   player.start(project,0,p);

   const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(3);

   while(player.active()&&std::chrono::steady_clock::now()<limit)

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

   player.stop();

   check(player.error().empty(),"callback stream playback failed");

   check(player.positionSeconds()>=.099,"callback did not deliver all project frames");

   check(player.diagnostics().bufferCapacityFrames==audioBufferSettings(p).capacityFrames,"wrong capacity");

  }

  // Deliberately accelerate fake device clock so the producer cannot keep up.

  // An underrun must be counted and playback must recover, not crash or stall.

  check(setenv("WENUY_FAKE_FAST_CALLBACK","1",1)==0,"setenv fast callback");

  Project stressed=project;stressed.durationSeconds=2.0;

  stressed.tracks.front().notes.front().durationSeconds=1.9;

  Playback speedTest;

  speedTest.start(stressed,0,BufferPreset::Low);

  const auto speedLimit=std::chrono::steady_clock::now()+std::chrono::seconds(4);

  while(speedTest.active()&&std::chrono::steady_clock::now()<speedLimit)

   std::this_thread::sleep_for(std::chrono::milliseconds(1));

  speedTest.stop();

  check(speedTest.error().empty(),"stressed audio callback error");

  check(speedTest.positionSeconds()>=1.99,"stressed callback did not drain fully");

  check(speedTest.diagnostics().underruns>0,"intentional software underruns were not recorded");

  check(unsetenv("WENUY_FAKE_FAST_CALLBACK")==0,"unsetenv fast callback");

  std::cout<<"0.4.1 SPSC wrap/underflow/concurrency, stress underruns and simulated playback passed\n";

  return 0;

 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

}
