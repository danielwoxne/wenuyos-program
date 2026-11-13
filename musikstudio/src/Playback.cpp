// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Playback.hpp"

#include "wenuystudio/BlockMixer.hpp"
#include "wenuystudio/LiveSynth.hpp"

#include <algorithm>

#include <array>

#include <cmath>

#include <dlfcn.h>

#include <stdexcept>

#include <thread>

#include <filesystem>

#include <vector>

#include <unistd.h>

#include <fcntl.h>

namespace wenuystudio {

namespace {

using PaError=int;using Stream=void;

struct PortAudio {

 void* so=nullptr;

 PaError(*init)()=nullptr;PaError(*terminate)()=nullptr;

 PaError(*open)(Stream**,int,int,unsigned long,double,unsigned long,void*,void*)=nullptr;

 PaError(*start)(Stream*)=nullptr;PaError(*stop)(Stream*)=nullptr;

 PaError(*write)(Stream*,const void*,unsigned long)=nullptr;

 PaError(*read)(Stream*,void*,unsigned long)=nullptr;

 PaError(*close)(Stream*)=nullptr;

 const char*(*errorText)(PaError)=nullptr;

 template<class T>void bind(T& fn,const char* name){fn=reinterpret_cast<T>(dlsym(so,name));if(!fn)throw std::runtime_error(std::string("Missing PortAudio API: ")+name);}

 PortAudio(){

  so=dlopen("libportaudio.so.2",RTLD_NOW|RTLD_LOCAL);

  if(!so)throw std::runtime_error("PortAudio is not installed");

  try{bind(init,"Pa_Initialize");bind(terminate,"Pa_Terminate");bind(open,"Pa_OpenDefaultStream");

      bind(start,"Pa_StartStream");bind(stop,"Pa_StopStream");

      bind(write,"Pa_WriteStream");bind(read,"Pa_ReadStream");

      bind(close,"Pa_CloseStream");bind(errorText,"Pa_GetErrorText");

      check(init(),"Initialize sound");}

  catch(...){dlclose(so);so=nullptr;throw;}

 }

 void check(PaError rc,const char* action){if(rc<0)throw std::runtime_error(std::string(action)+": "+errorText(rc));}

 ~PortAudio(){if(so){terminate();dlclose(so);}}

};

struct SoundStream{

 PortAudio& pa;Stream* stream=nullptr;bool started=false;

 SoundStream(PortAudio& api,uint32_t rate,unsigned long frames=512):pa(api){pa.check(pa.open(&stream,0,2,1,double(rate),frames,nullptr,nullptr),"Open sound output");pa.check(pa.start(stream),"Start sound output");started=true;}

 ~SoundStream(){if(stream){if(started)pa.stop(stream);pa.close(stream);}}

};

}


bool LivePreview::send(Event e)noexcept{
 if(!running_)return false;
 const unsigned w=written_.load(std::memory_order_relaxed);
 if(w-read_.load(std::memory_order_acquire)>=capacity){
  // Safe fallback: an overflowing NoteOff must never leave a stuck voice.
  panic_.store(true,std::memory_order_release);
  return false;
 }
 events_[w%capacity]=e;written_.store(w+1,std::memory_order_release);return true;
}
bool LivePreview::noteOn(int key,int instrument,float velocity)noexcept{
 if(key<0||key>127||!std::isfinite(velocity)||velocity<=0||velocity>1||
    instrument<0||instrument>15)return false;
 return send({Kind::On,key,instrument,velocity});
}
bool LivePreview::noteOff(int key)noexcept{
 return key>=0&&key<=127&&send({Kind::Off,key,0,0});
}
bool LivePreview::allNotesOff()noexcept{return send({Kind::AllOff,0,0,0});}
void LivePreview::start(std::uint32_t rate){
 if(rate<8000||rate>192000)throw std::invalid_argument("Invalid preview sample rate");
 if(running_)return;
 if(worker_.joinable())worker_.join();
 {std::lock_guard<std::mutex> lock(mutex_);error_.clear();}
 written_=0;read_=0;panic_=false;rate_=rate;running_=true;
 try{worker_=std::thread(&LivePreview::run,this);}
 catch(...){running_=false;throw;}
}
void LivePreview::stop()noexcept{
 running_=false;if(worker_.joinable())worker_.join();
 written_=0;read_=0;panic_=false;
}
std::string LivePreview::error()const{
 std::lock_guard<std::mutex> lock(mutex_);return error_;
}
void LivePreview::run()noexcept{
 try{
  PortAudio pa;SoundStream output(pa,rate_,256);
  LiveSynth synth;
  std::array<float,512> pcm{};
  while(running_){
   if(panic_.exchange(false,std::memory_order_acq_rel)){
    // Drop all older queued On events together with a lost Off.
    // Otherwise an old On processed after panic could stick forever.
    read_.store(written_.load(std::memory_order_acquire),std::memory_order_release);
    synth.allNotesOff();
   }
   for(unsigned i=0;i<128;++i){
    const unsigned r=read_.load(std::memory_order_relaxed);
    if(r==written_.load(std::memory_order_acquire))break;
    const auto ev=events_[r%capacity];
    read_.store(r+1,std::memory_order_release);
    switch(ev.kind){
     case Kind::On:synth.noteOn(ev.key,ev.instrument,ev.velocity);break;
     case Kind::Off:synth.noteOff(ev.key);break;
     case Kind::AllOff:synth.allNotesOff();break;
    }
   }
   synth.render(pcm.data(),256,rate_);
   pa.check(pa.write(output.stream,pcm.data(),256),"Write audition stream");
  }
 }catch(const std::exception& e){
  std::lock_guard<std::mutex> lock(mutex_);error_=e.what();
 }
 running_=false;
}

void Recording::start(const std::string& filename,std::uint32_t rate){

 if(rate<8000||rate>192000)throw std::invalid_argument("Invalid recording sample rate");

 if(std::filesystem::exists(filename))throw std::runtime_error("Recording destination already exists");

 stop();{std::lock_guard<std::mutex> lock(mutex_);error_.clear();}

 completed_=false;active_=true;

 try{thread_=std::thread(&Recording::run,this,filename,rate);}

 catch(...){active_=false;throw;}

}

void Recording::stop(){active_=false;if(thread_.joinable())thread_.join();}

std::string Recording::error()const{std::lock_guard<std::mutex> lock(mutex_);return error_;}

void Recording::run(std::string target,uint32_t rate)noexcept {

 std::string temp;

 try{

  // mkstemp creates a unique file with restrictive permissions, which is

  // never the destination file. On errors the partial recording is deleted.

  std::string prefix=target+".part-XXXXXX";

  std::vector<char> pattern(prefix.begin(),prefix.end());pattern.push_back('\0');

  int fd=mkstemp(pattern.data());

  if(fd<0)throw std::runtime_error("Cannot create temporary recording");

  temp=pattern.data();close(fd);

  PortAudio pa;Stream* raw=nullptr;

  pa.check(pa.open(&raw,1,0,1,double(rate),512,nullptr,nullptr),"Open microphone");

  struct StreamGuard {PortAudio& pa;Stream* s;bool started=false;

   ~StreamGuard(){if(s){if(started)pa.stop(s);pa.close(s);}}} guard{pa,raw,false};

  pa.check(pa.start(raw),"Start microphone");guard.started=true;

  WaveWriter writer(temp,rate);

  std::array<float,512> mono{};std::array<Stereo,512> stereo{};

  while(active_){

   pa.check(pa.read(raw,mono.data(),512),"Read microphone");

   for(std::size_t i=0;i<mono.size();++i)stereo[i]={mono[i],mono[i]};

   writer.append(stereo.data(),stereo.size());

  }

  writer.finish();

  if(std::filesystem::exists(target))throw std::runtime_error("Recording target appeared during capture");

  // A hard link is no-replace on POSIX filesystems; source and dest share the same FS.

  if(::link(temp.c_str(),target.c_str())!=0)throw std::runtime_error("Could not finalize recording without overwrite");

  std::filesystem::remove(temp);temp.clear();completed_=true;

 }catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex_);error_=e.what();}

 if(!temp.empty()){std::error_code ignored;std::filesystem::remove(temp,ignored);}

 active_=false;

}

void Playback::start(const Project& project,double startAt){

 if(!std::isfinite(startAt)||startAt<0||startAt>=project.durationSeconds)throw std::invalid_argument("Invalid playhead");

 stop();{std::lock_guard<std::mutex> lock(mutex_);error_.clear();}

 rate_.store(project.sampleRate);frame_.store(uint64_t(startAt*project.sampleRate));left_=0;right_=0;active_=true;

 try{thread_=std::thread(&Playback::run,this,project,startAt);}catch(...){active_=false;throw;}

}

void Playback::stop(){active_=false;if(thread_.joinable())thread_.join();left_=right_=0;}

std::string Playback::error()const{std::lock_guard<std::mutex> l(mutex_);return error_;}

void Playback::run(Project project,double startAt)noexcept{

 try{

  const auto loop=project.loop;

  BlockMixer mixer(project);

  PortAudio pa;SoundStream output(pa,project.sampleRate);

  std::array<Stereo,512> block{};

  std::array<float,1024> interleaved{};

  uint64_t pos=uint64_t(startAt*project.sampleRate),end=mixer.totalFrames();

  const uint64_t beginLoop=uint64_t(loop.startSeconds*project.sampleRate);

  const uint64_t endLoop=uint64_t(loop.endSeconds*project.sampleRate);

  const bool repeat=loop.enabled&&endLoop>beginLoop&&endLoop<=end;

  if(repeat&&pos>=endLoop)pos=beginLoop;

  while(active_){

   if(pos>=end){if(!repeat)break;pos=beginLoop;}

   if(repeat&&pos>=endLoop)pos=beginLoop;

   uint64_t limit=repeat?endLoop:end;

   auto n=std::size_t(std::min<uint64_t>(512,limit-pos));

   if(!n)break;

   BlockMeter meter{};mixer.process(pos,block.data(),n,&meter);

   left_=meter.left;right_=meter.right;

   for(std::size_t i=0;i<n;++i){interleaved[2*i]=block[i].l;interleaved[2*i+1]=block[i].r;}

   pa.check(pa.write(output.stream,interleaved.data(),static_cast<unsigned long>(n)),"Write audio");

   pos+=n;frame_=pos;

  }

 }catch(const std::exception& e){if(active_){std::lock_guard<std::mutex> l(mutex_);error_=e.what();}}

 active_=false;

}

}
