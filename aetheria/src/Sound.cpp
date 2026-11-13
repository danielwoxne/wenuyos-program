#include "Sound.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace aetheria {
namespace {
constexpr int sampleRate=22050;
constexpr float pi=3.14159265358979f;
struct Tone {float base, duration, amplitude, glide;};
Tone tone(Sound::Cue c){
  switch(c){
    case Sound::Cue::Strike: return {190.f,.095f,.19f,-90.f};
    case Sound::Cue::Magic: return {440.f,.26f,.20f,360.f};
    case Sound::Cue::Crystal: return {760.f,.16f,.14f,430.f};
    case Sound::Cue::Healing: return {480.f,.22f,.14f,210.f};
    case Sound::Cue::Dialogue:return {560.f,.055f,.065f,-80.f};
    case Sound::Cue::Craft: return {340.f,.28f,.17f,480.f};
    case Sound::Cue::Error: return {180.f,.14f,.08f,-55.f};
    case Sound::Cue::Damage:return {110.f,.15f,.18f,-75.f};
  }
  return {440.f,.1f,.1f,0.f};
}
}
Sound::~Sound(){stop();}
void Sound::stop(){if(device_){SDL_CloseAudioDevice(device_);device_=0;}}
void Sound::start(){
  if(SDL_InitSubSystem(SDL_INIT_AUDIO)!=0)return;
  SDL_AudioSpec desired{};
  desired.freq=sampleRate;
  desired.format=AUDIO_S16LSB;
  desired.channels=1;
  desired.samples=512;
  SDL_AudioSpec obtained{};
  device_=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,0);
  if(!device_ || obtained.freq!=sampleRate || obtained.channels!=1 || obtained.format!=AUDIO_S16LSB){
    if(device_)SDL_CloseAudioDevice(device_);
    device_=0;return;
  }
  SDL_PauseAudioDevice(device_,0);
}
void Sound::play(Cue cue){
  if(!enabled() || SDL_GetQueuedAudioSize(device_)>sampleRate*2/3)return;
  const auto t=tone(cue);
  const int samples=std::max(1,static_cast<int>(sampleRate*t.duration));
  std::vector<int16_t> audio(static_cast<size_t>(samples));
  for(int i=0;i<samples;++i){
    const float progress=float(i)/float(samples);
    const float seconds=float(i)/sampleRate;
    const float phase=2.f*pi*(t.base*seconds+.5f*t.glide*seconds*progress);
    const float envelope=std::min(1.f,progress*35.f)*std::pow(1.f-progress,1.8f);
    const float wave=std::sin(phase)+.23f*std::sin(phase*2.01f);
    audio[size_t(i)]=static_cast<int16_t>(std::clamp(wave*envelope*t.amplitude,-.99f,.99f)*32760.f);
  }
  SDL_QueueAudio(device_,audio.data(),static_cast<uint32_t>(audio.size()*sizeof(int16_t)));
}
}
