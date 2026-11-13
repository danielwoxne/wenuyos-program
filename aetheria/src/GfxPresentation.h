#pragma once
// Optional Aetheria -> system WenuyGFX 0.9 CPU rendering adapter.
#include "Scene3D.h"
#include <wenuygfx/Graphics.hpp>
#include <wenuygfx/Platform.hpp>

namespace aetheria {
class GfxPresentation {
public:
  GfxPresentation();
  ~GfxPresentation();
  GfxPresentation(const GfxPresentation&)=delete;
  GfxPresentation& operator=(const GfxPresentation&)=delete;
  bool draw(SDL_Renderer* renderer,const legends::World& world,
            Scene3D& source,float yaw,bool lowDetail,
            int logicalWidth,int logicalHeight,SceneStats& stats,float cameraZoom=1.f);
  void reset() noexcept; // call before SDL renderer/window is destroyed
private:
  wenuygfx::SoftwareRenderer software_;
  wenuygfx::Scene scene_;
  wenuygfx::Camera camera_;
  wenuygfx::Frame frame_;
  SDL_Texture* texture_=nullptr;
  int textureWidth_=0,textureHeight_=0;
  bool lowMemory_=false;
};
}
