#pragma once
#include "Model.h"
#include "MiniSDL.h"
#include <cstddef>

namespace aetheria {
struct SceneStats { int terrainTiles=0, triangles=0; };
// A genuine perspective-projected 3D triangle scene, using SDL2's GPU/software
// geometry backend. No GL context, external textures, or VRAM assumptions.
class Scene3D {
 public:
  SceneStats render(SDL_Renderer* renderer, const legends::World& world,
                    float cameraYaw, bool lowDetail, int width, int height);
};
}
