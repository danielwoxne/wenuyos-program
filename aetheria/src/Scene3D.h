#pragma once
#include "Model.h"
#include "MiniSDL.h"
#include <cstddef>
#ifdef AETHERIA_WITH_WENUYGFX
#include <wenuygfx/Graphics.hpp>
#endif

namespace aetheria {
struct SceneStats { int terrainTiles=0, triangles=0; };
// A genuine perspective-projected 3D triangle scene, using SDL2's GPU/software
// geometry backend. No GL context, external textures, or VRAM assumptions.
class Scene3D {
 public:
  SceneStats render(SDL_Renderer* renderer, const legends::World& world,
                    float cameraYaw, bool lowDetail, int width, int height, float zoom=1.f);
#ifdef AETHERIA_WITH_WENUYGFX
  // Build the very same terrain/character triangles for the system renderer.
  // No GPU context, external file or resource allocation is needed.
  SceneStats collectWenuyGfx(const legends::World& world,float cameraYaw,
      bool lowDetail,int width,int height,
      wenuygfx::Scene& scene,wenuygfx::Camera& camera,float zoom=1.f);
#endif
};
}
