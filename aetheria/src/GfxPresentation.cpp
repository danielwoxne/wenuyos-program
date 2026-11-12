#include "GfxPresentation.h"
#include <algorithm>
#include <cstdint>

namespace aetheria {
GfxPresentation::GfxPresentation()
  :lowMemory_(wenuygfx::currentRuntimeProfile().lowMemory){}
GfxPresentation::~GfxPresentation(){reset();}
void GfxPresentation::reset() noexcept {
  if(texture_) SDL_DestroyTexture(texture_);
  texture_=nullptr;
  textureWidth_=textureHeight_=0;
  frame_.argb.clear();
  scene_.triangles.clear();
}
bool GfxPresentation::draw(SDL_Renderer* renderer,const legends::World& world,
          Scene3D& source,float yaw,bool lowDetail,
          int logicalWidth,int logicalHeight,SceneStats& stats){
  if(!renderer||logicalWidth<32||logicalHeight<32)return false;
  // Fixed low resolution means bounded CPU cost and <2 MB for colour+depth.
  // Keep the original aspect ratio even when switching renderers.
  const int width=(lowMemory_||lowDetail)?480:640;
  const int height=static_cast<int>(static_cast<int64_t>(width)*logicalHeight/logicalWidth);
  if(height<16||height>480)return false;
  const auto next=source.collectWenuyGfx(world,yaw,lowMemory_||lowDetail,
                     logicalWidth,logicalHeight,scene_,camera_);
  if(scene_.triangles.empty()||scene_.triangles.size()>26000u)return false;
  auto visuals=wenuygfx::visualsForHour(14.f*std::clamp(world.daylight(),0.f,1.f));
  visuals.timeSeconds=world.time;
  visuals.fogStart=scene_.fogStart;
  visuals.fogEnd=scene_.fogEnd;
  if(!software_.setVisualSettings(visuals)||
     !software_.render(scene_,camera_,width,height,frame_))return false;
  if(!texture_||textureWidth_!=width||textureHeight_!=height){
    if(texture_)SDL_DestroyTexture(texture_);
    texture_=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,width,height);
    textureWidth_=texture_?width:0;
    textureHeight_=texture_?height:0;
    if(!texture_)return false;
  }
  if(SDL_UpdateTexture(texture_,nullptr,frame_.argb.data(),width*4)!=0 ||
     SDL_RenderCopy(renderer,texture_,nullptr,nullptr)!=0)return false;
  stats=next;
  return true;
}
}
