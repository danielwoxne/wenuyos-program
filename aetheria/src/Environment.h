#pragma once
// Deterministic world decoration shared by physics and renderer.
// No external assets, allocations or frame-dependent randomness.
#include "Model.h"
#include <cmath>
#include <cstdint>

namespace legends {
inline uint32_t terrainHash(uint32_t v) {
  v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;
  return v;
}
inline uint32_t terrainHashXY(int x,int y,uint32_t seed) {
  return terrainHash(static_cast<uint32_t>(x)*214013u ^
                     static_cast<uint32_t>(y)*2531011u ^ seed);
}
struct TreePosition {float x=0,y=0,scale=1;bool exists=false;};
inline TreePosition treeAt(const World& w,int x,int y) {
  const Tile t=w.tile(x,y);
  if(t!=Tile::Grass && t!=Tile::Forest)return {};
  const uint32_t h=terrainHashXY(x,y,w.seed());
  if(h%(t==Tile::Forest?7u:19u)!=0u)return {};
  if(std::hypot(x+.5f-w.shrine.x,y+.5f-w.shrine.y)<=4.3f)return {};
  return {x+.3f+((h>>8)%41)*.01f,y+.3f+((h>>16)%41)*.01f,
          .7f+(h%60)*.012f,true};
}
}
