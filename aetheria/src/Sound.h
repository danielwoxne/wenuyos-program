#pragma once
#include "MiniSDL.h"
namespace aetheria {
// Optional SDL2-generated UI/SFX. No audio files, no network, no music licensing.
class Sound {
public:
  enum class Cue { Strike, Magic, Crystal, Healing, Dialogue, Craft, Error, Damage };
  ~Sound();
  void start();
  void stop();
  void play(Cue cue);
  bool enabled() const {return device_!=0 && !muted_;}
  bool toggleMute() {muted_=!muted_;return !muted_;}
private:
  unsigned int device_=0;
  bool muted_=false;
};
}
