// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/AudioIO.hpp"

#include "wenuystudio/BlockMixer.hpp"

#include "wenuystudio/Wave.hpp"

#include <algorithm>

#include <array>

#include <cmath>

#include <cstdio>

#include <cerrno>

#include <system_error>

#include <cstring>

#include <dlfcn.h>

#include <filesystem>

#include <stdexcept>

#include <string>

#include <unistd.h>

#include <vector>

#include <chrono>

#include <thread>

namespace wenuystudio {

namespace {

namespace fs=std::filesystem;

using PaError=int;

using PaStream=void;

struct PaStreamParameters {

 int device;

 int channelCount;

 unsigned long sampleFormat;

 double suggestedLatency;

 void* hostApiSpecificStreamInfo;

};

struct PaCallbackTimeInfo {

 double inputBufferAdcTime;

 double currentTime;

 double outputBufferDacTime;

};

using PaCallback=int (*)(const void*,void*,unsigned long,

 const PaCallbackTimeInfo*,unsigned long,void*);

constexpr int paContinue=0;

constexpr int paComplete=1;

// Public PortAudio v19 ABI structs, mirrored solely for runtime diagnostics.

// PortAudio headers are deliberately not required for the C++ core.

struct PaDeviceInfo {

 int structVersion;

 const char* name;

 int hostApi;

 int maxInputChannels;

 int maxOutputChannels;

 double defaultLowInputLatency;

 double defaultLowOutputLatency;

 double defaultHighInputLatency;

 double defaultHighOutputLatency;

 double defaultSampleRate;

};

struct PaHostApiInfo {

 int structVersion;

 int type;

 const char* name;

 int deviceCount;

 int defaultInputDevice;

 int defaultOutputDevice;

};

constexpr unsigned long paFloat32=0x00000001;

constexpr PaError paInputOverflowed=-9981;

constexpr PaError paOutputUnderflowed=-9980;

struct PortAudio {

 void* library=nullptr;

 PaError (*init)()=nullptr;

 PaError (*terminate)()=nullptr;

 PaError (*open)(PaStream**,int,int,unsigned long,double,unsigned long,void*,void*)=nullptr;

 PaError (*openStream)(PaStream**,const PaStreamParameters*,const PaStreamParameters*,

                       double,unsigned long,unsigned long,PaCallback,void*)=nullptr;

 PaError (*isStreamActive)(PaStream*)=nullptr;

 PaError (*start)(PaStream*)=nullptr;

 PaError (*stop)(PaStream*)=nullptr;

 PaError (*close)(PaStream*)=nullptr;

 PaError (*read)(PaStream*,void*,unsigned long)=nullptr;

 PaError (*write)(PaStream*,const void*,unsigned long)=nullptr;

 const char* (*errorText)(PaError)=nullptr;

 int (*getDeviceCount)()=nullptr;

 int (*getDefaultInputDevice)()=nullptr;

 int (*getDefaultOutputDevice)()=nullptr;

 const PaDeviceInfo* (*getDeviceInfo)(int)=nullptr;

 const PaHostApiInfo* (*getHostApiInfo)(int)=nullptr;

 template<typename F> void load(F& target,const char* name){

  void* s=dlsym(library,name);

  if(!s)throw std::runtime_error(std::string("Missing PortAudio symbol: ")+name);

  static_assert(sizeof(target)==sizeof(s),"Function pointers incompatible with dlsym");

  std::memcpy(&target,&s,sizeof(s));

 }

 PortAudio(){

  library=dlopen("libportaudio.so.2",RTLD_NOW|RTLD_LOCAL);

  if(!library)throw std::runtime_error("PortAudio library not installed (libportaudio.so.2)");

  try{

   load(init,"Pa_Initialize");load(terminate,"Pa_Terminate");

   load(open,"Pa_OpenDefaultStream");load(openStream,"Pa_OpenStream");

   load(isStreamActive,"Pa_IsStreamActive");load(start,"Pa_StartStream");

   load(stop,"Pa_StopStream");load(close,"Pa_CloseStream");

   load(read,"Pa_ReadStream");load(write,"Pa_WriteStream");

   load(errorText,"Pa_GetErrorText");

   load(getDeviceCount,"Pa_GetDeviceCount");

   load(getDefaultInputDevice,"Pa_GetDefaultInputDevice");

   load(getDefaultOutputDevice,"Pa_GetDefaultOutputDevice");

   load(getDeviceInfo,"Pa_GetDeviceInfo");

   load(getHostApiInfo,"Pa_GetHostApiInfo");

   check(init(),"Initialize PortAudio");

  }catch(...){dlclose(library);library=nullptr;throw;}

 }

 void check(PaError code,const char* operation) const {

  if(code!=0)throw std::runtime_error(std::string(operation)+": "+(errorText?errorText(code):"error"));

 }

 ~PortAudio(){if(library){terminate();dlclose(library);}}

};

struct Stream {

 PortAudio& api;

 PaStream* ptr=nullptr;

 bool started=false;

 Stream(PortAudio& a,int inputs,int outputs,double rate):api(a){

  api.check(api.open(&ptr,inputs,outputs,paFloat32,rate,512,nullptr,nullptr),"Open default audio device");

  try{api.check(api.start(ptr),"Start audio stream");started=true;}

  catch(...){api.close(ptr);throw;}

 }

 ~Stream(){if(ptr){if(started)api.stop(ptr);api.close(ptr);}}

};

// Only the worker may call the mixer. The sound callback only pulls frames

// from this bounded SPSC ring. It never allocates, blocks, takes a mutex,

// logs, calls GTK, or touches disk/mixer. The callback state's lifetime is

// longer than the PortAudio stream's lifetime.

struct CallbackState {

 AudioBuffer& buffer;

 const std::atomic<bool>& playing;

 std::atomic<bool> producerDone{false};

 std::atomic<bool> finished{false};

 std::atomic<std::uint64_t>& position;

 std::atomic<std::uint64_t>& delivered;

 std::atomic<std::uint64_t>& underruns;

 std::atomic<std::uint32_t>& buffered;

 std::atomic<float>& left;

 std::atomic<float>& right;

 explicit CallbackState(AudioBuffer& b,const std::atomic<bool>& a,

   std::atomic<std::uint64_t>& pos,std::atomic<std::uint64_t>& received,std::atomic<std::uint64_t>& u,

   std::atomic<std::uint32_t>& fill,std::atomic<float>& l,std::atomic<float>& r)

  :buffer(b),playing(a),position(pos),delivered(received),underruns(u),buffered(fill),left(l),right(r){}

};

int playbackCallback(const void*,void* output,unsigned long count,

                     const PaCallbackTimeInfo*,unsigned long,void* user) noexcept {

 auto& s=*static_cast<CallbackState*>(user);

 auto* out=static_cast<float*>(output);

 // PortAudio guarantees the output pointer is valid for requested frames.

 if(!s.playing.load(std::memory_order_relaxed)){

  std::fill_n(out,static_cast<std::size_t>(count)*2,0.0f);

  s.finished.store(true,std::memory_order_release);

  return paComplete;

 }

 float l=0,r=0;

 const auto n=s.buffer.popInterleaved(out,count,l,r);

 s.position.fetch_add(n,std::memory_order_relaxed);

 s.delivered.fetch_add(n,std::memory_order_relaxed);

 s.buffered.store(static_cast<std::uint32_t>(s.buffer.available()),std::memory_order_relaxed);

 s.left.store(l,std::memory_order_relaxed);

 s.right.store(r,std::memory_order_relaxed);

 const bool done=s.producerDone.load(std::memory_order_acquire);

 if(n<count && !done)s.underruns.fetch_add(1,std::memory_order_relaxed);

 if(done && s.buffer.available()==0){

  s.finished.store(true,std::memory_order_release);

  return paComplete;

 }

 return paContinue;

}

struct OutputStream {

 PortAudio& api;

 PaStream* ptr=nullptr;

 bool started=false;

 OutputStream(PortAudio& a,std::uint32_t rate,BufferSettings settings,CallbackState& state):api(a){

  const auto device=api.getDefaultOutputDevice();

  if(device<0)throw std::runtime_error("No default output device");

  const auto* info=api.getDeviceInfo(device);

  if(!info||info->maxOutputChannels<2)

   throw std::runtime_error("Default output device does not provide stereo");

  PaStreamParameters output{device,2,paFloat32,

     settings.callbackFrames<=256?info->defaultLowOutputLatency:info->defaultHighOutputLatency,nullptr};

  api.check(api.openStream(&ptr,nullptr,&output,static_cast<double>(rate),

                          static_cast<unsigned long>(settings.callbackFrames),0,

                          playbackCallback,&state),"Open buffered audio output");

 }

 void start(){api.check(api.start(ptr),"Start buffered audio");started=true;}

 ~OutputStream(){if(ptr){if(started)api.stop(ptr);api.close(ptr);}}

};

void playProjectBuffered(PortAudio& api,BlockMixer& mixer,std::uint64_t from,

              const std::atomic<bool>& active,std::atomic<std::uint64_t>& position,

              std::atomic<std::uint64_t>& delivered,std::atomic<std::uint64_t>& underruns,

              std::atomic<std::uint32_t>& buffered,

              std::atomic<float>& peakLeft,std::atomic<float>& peakRight,

              BufferSettings settings){

 AudioBuffer ring(settings.capacityFrames);

 CallbackState state(ring,active,position,delivered,underruns,buffered,peakLeft,peakRight);

 std::array<Stereo,1024> samples{};

 std::uint64_t next=std::min(from,mixer.totalFrames());

 const auto fill=[&](std::size_t target){

  while(active.load(std::memory_order_relaxed)&&next<mixer.totalFrames()&&

        ring.available()<target&&ring.freeFrames()>=512){

   const auto wanted=static_cast<std::size_t>(std::min<std::uint64_t>(

      {1024,mixer.totalFrames()-next,ring.freeFrames()}));

   const auto n=mixer.process(next,samples.data(),wanted);

   if(n==0)break;

   const auto pushed=ring.push(samples.data(),n);

   if(pushed!=n)throw std::runtime_error("Audio buffer internal producer mismatch");

   next+=n;

  }

  if(next==mixer.totalFrames())state.producerDone.store(true,std::memory_order_release);

  buffered.store(static_cast<std::uint32_t>(ring.available()),std::memory_order_relaxed);

 };

 // Pre-fill before the device starts. No disk read happens in its callback.

 fill(settings.prefillFrames);

 if(!active.load(std::memory_order_relaxed))return;

 OutputStream stream(api,mixer.sampleRate(),settings,state);

 stream.start();

 while(active.load(std::memory_order_relaxed)&&

       !state.finished.load(std::memory_order_acquire)){

  fill(settings.capacityFrames);

  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  const auto streamActive=api.isStreamActive(stream.ptr);

  if(streamActive<0)api.check(streamActive,"Audio output stream status");

  if(streamActive==0 && !state.finished.load(std::memory_order_acquire))

   throw std::runtime_error("Audio device stopped unexpectedly");

 }

}

}

AudioDeviceReport queryAudioDevices(){

 PortAudio api;

 AudioDeviceReport report;

 const int count=api.getDeviceCount();

 if(count<0)api.check(count,"List audio devices");

 // Reject implausible driver responses; avoid unbounded allocations.

 if(count>4096)throw std::runtime_error("Audio driver reported too many devices");

 report.defaultInput=api.getDefaultInputDevice();

 report.defaultOutput=api.getDefaultOutputDevice();

 report.devices.reserve(static_cast<std::size_t>(count));

 for(int i=0;i<count;++i){

  const PaDeviceInfo* d=api.getDeviceInfo(i);

  if(!d)continue; // A device may disappear during enumeration.

  const PaHostApiInfo* host=api.getHostApiInfo(d->hostApi);

  AudioDevice item;

  item.index=i;

  item.name=d->name?d->name:"(unnamed device)";

  item.hostApi=host&&host->name?host->name:"(unknown API)";

  item.inputChannels=d->maxInputChannels;

  item.outputChannels=d->maxOutputChannels;

  item.defaultSampleRate=d->defaultSampleRate;

  item.defaultInput=(i==report.defaultInput);

  item.defaultOutput=(i==report.defaultOutput);

  report.devices.emplace_back(std::move(item));

 }

 return report;

}

AudioProbeReport probeDefaultAudioDevices(){

 PortAudio api;

 AudioProbeReport report;

 // Check output and input separately so a failed microphone doesn't hide

 // successful speaker initialization, or vice versa.

 const auto tryStream=[&api](int channelsIn,int channelsOut,int idx){

  if(idx<0)throw std::runtime_error("No default audio device selected");

  const PaDeviceInfo* device=api.getDeviceInfo(idx);

  if(!device)throw std::runtime_error("Default device was removed");

  double rate=device->defaultSampleRate;

  if(!std::isfinite(rate)||rate<8000||rate>192000)rate=48000;

  Stream test(api,channelsIn,channelsOut,rate);

 };

 try{

  tryStream(0,2,api.getDefaultOutputDevice());

  report.outputOpened=true;

 }catch(const std::exception& e){report.outputError=e.what();}

 try{

  tryStream(1,0,api.getDefaultInputDevice());

  report.inputOpened=true;

 }catch(const std::exception& e){report.inputError=e.what();}

 return report;

}

BufferSettings audioBufferSettings(BufferPreset preset) noexcept {

 switch(preset){

  case BufferPreset::Low:return {128,4096,512};

  case BufferPreset::Balanced:return {512,8192,2048};

  case BufferPreset::Safe:return {1024,16384,4096};

 }

 return {512,8192,2048};

}

Playback::~Playback(){stop();}

void Playback::start(Project project,double fromSeconds,BufferPreset preset){

 if(!std::isfinite(fromSeconds)||fromSeconds<0||fromSeconds>project.durationSeconds)

  throw std::invalid_argument("Invalid playback start");

 stop();{

  std::lock_guard<std::mutex> guard(errorMutex_);error_.clear();

 }

 rate_.store(project.sampleRate);

 position_.store(static_cast<std::uint64_t>(fromSeconds*project.sampleRate));

 peakLeft_.store(0);peakRight_.store(0);underruns_.store(0);delivered_.store(0);buffered_.store(0);

 const auto settings=audioBufferSettings(preset);

 bufferCapacity_.store(static_cast<std::uint32_t>(settings.capacityFrames));

 callbackFrames_.store(static_cast<std::uint32_t>(settings.callbackFrames));

 active_.store(true);

 try{worker_=std::thread(&Playback::run,this,std::move(project),fromSeconds,preset);}

 catch(...){active_.store(false);throw;}

}

void Playback::stop(){active_.store(false);if(worker_.joinable())worker_.join();}

std::string Playback::error() const {std::lock_guard<std::mutex> lock(errorMutex_);return error_;}

double Playback::positionSeconds() const noexcept{return double(position_.load())/rate_.load();}

PlaybackDiagnostics Playback::diagnostics() const noexcept {

 const auto rate=rate_.load();

 const auto capacity=bufferCapacity_.load();

 const auto frames=buffered_.load();

 return {delivered_.load(),underruns_.load(),frames,capacity,

  rate?1000.0*frames/rate:0.0,

  rate?1000.0*callbackFrames_.load()/rate:0.0};

}

void Playback::run(Project project,double fromSeconds,BufferPreset preset) noexcept {

 try{

  BlockMixer mixer(std::move(project));

  PortAudio api;

  playProjectBuffered(api,mixer,static_cast<std::uint64_t>(fromSeconds*mixer.sampleRate()),

              active_,position_,delivered_,underruns_,buffered_,peakLeft_,peakRight_,

              audioBufferSettings(preset));

 }catch(const std::exception& e){

  if(active_.load()){std::lock_guard<std::mutex> lock(errorMutex_);error_=e.what();}

 }

 active_.store(false);

}

Recording::~Recording(){stop();}

void Recording::start(const std::string& filename,std::uint32_t rate){

 if(rate<8000||rate>192000)throw std::invalid_argument("Invalid recording sample rate");

 stop();

 if(fs::exists(filename))throw std::runtime_error("Recording destination already exists");

 {std::lock_guard<std::mutex> lock(errorMutex_);error_.clear();}

 frames_.store(0);active_.store(true);

 try{worker_=std::thread(&Recording::run,this,filename,rate);}

 catch(...){active_.store(false);throw;}

}

void Recording::stop(){active_.store(false);if(worker_.joinable())worker_.join();}

std::string Recording::error() const {std::lock_guard<std::mutex> lock(errorMutex_);return error_;}

void Recording::run(std::string filename,std::uint32_t rate) noexcept {

 std::string part;

 try{

  // Unique same-directory partial file; no two sessions share a .part file.

  std::string pattern=filename+".part-XXXXXX";

  std::vector<char> path(pattern.begin(),pattern.end());path.push_back('\0');

  const int fd=mkstemp(path.data());

  if(fd<0)throw std::runtime_error("Cannot reserve recording temporary file");

  close(fd);part=path.data();

  PortAudio api;

  // Request mono first: works with many laptop microphones and saves disk space.

  // Stored WAV remains stereo via dual mono for editing and mix compatibility.

  Stream stream(api,1,0,rate);

  WaveWriter writer(part,rate);

  std::array<float,512> input{};

  std::array<Stereo,512> output{};

  while(active_.load()){

   const auto code=api.read(stream.ptr,input.data(),512);

   if(!active_.load())break;

   if(code!=0&&code!=paInputOverflowed)api.check(code,"Recording input");

   for(std::size_t i=0;i<512;++i)output[i]={input[i],input[i]};

   writer.append(output.data(),512);

   frames_.fetch_add(512);

  }

  writer.finish();

  if(frames_.load()==0)throw std::runtime_error("Recording was empty");

  // Atomic publish without overwrite, even if another process creates

  // destination after Recording::start() did the initial check.

  if(::link(part.c_str(),filename.c_str())!=0){

   if(errno==EEXIST)throw std::runtime_error("Recording destination already exists: "+filename);

   throw std::system_error(errno,std::generic_category(),"Cannot publish recording");

  }

  std::error_code cleanup;fs::remove(part,cleanup);

 }catch(const std::exception& e){

  std::lock_guard<std::mutex> lock(errorMutex_);error_=e.what();

  if(!part.empty()){std::error_code ec;fs::remove(part,ec);}

 }

 active_.store(false);

}

}
