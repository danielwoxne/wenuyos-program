#include "Model.h"
#include "Environment.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace legends;
int main() {
  World w;
  assert(w.questStage()==0 && w.collected==0 && w.defeated==0);
  assert(!w.blocked(40,42) && w.blocked(-1,-1));
  assert(w.tile(-1,0)==Tile::Water);
  assert(!w.potion());
  const auto element=w.hero.element;w.cycleElement();assert(w.hero.element!=element);
  Input in;in.right=true;w.update(.05f,in);assert(w.hero.pos.x>40.f);
  const float x=w.hero.pos.x;w.paused=true;w.update(.05f,in);assert(w.hero.pos.x==x);w.paused=false;
  assert(w.skill());assert(!w.skill());w.update(.05f,{});
  auto p=std::filesystem::temp_directory_path()/"aetheria-model-test.save";
  assert(w.save(p.string()));World copy;assert(copy.load(p.string()));
  assert(std::abs(copy.hero.pos.x-w.hero.pos.x)<.001f);
  assert(copy.hero.element==w.hero.element);
  {std::ofstream out(p,std::ios::trunc);out<<"not a valid save\n";}
  assert(!copy.load(p.string()));
  std::error_code ec;std::filesystem::remove(p,ec);
  // Pickup and quest transitions.
  w.reset();for(auto& c:w.crystals) { w.hero.pos=c.pos; w.update(.01f,{}); }
  assert(w.collected==static_cast<int>(w.crystals.size()));assert(w.questStage()==1);
  w.hero.pos=w.shrine;assert(!w.interact());
  for(int i=0;i<4;++i) {w.enemies[i].alive=false;++w.defeated;}
  assert(w.questStage()==2);assert(w.interact()&&w.won&&w.questStage()==3);
  // Continuous deterministic height field: mesh must not crack at shared vertices.
  assert(std::isfinite(w.height(10.25f,15.4f)));
  assert(w.height(27.f,18.f)==w.height(27.f,18.f));
  assert(w.height(30.125f,40.f)>0.f);
  // Camera-relative analog movement supports vectors and retains axis collision.
  w.reset(); Input oriented;oriented.analog=true;oriented.moveY=-1;
  const float origY=w.hero.pos.y; w.update(.05f,oriented);
  assert(w.hero.pos.y<origY);
  // A single press initiates one jump; a held key cannot double-jump.
  w.reset();for(auto& e:w.enemies)e.alive=false;Input jump;jump.jump=true;w.update(.05f,jump);
  assert(w.hero.air>0 && w.hero.verticalSpeed>0);
  float apex=0;
  for(int i=0;i<200;++i){w.update(.05f,jump);apex=std::max(apex,w.hero.air);}
  assert(apex>.3f && w.hero.air==0.f && w.hero.verticalSpeed==0.f);
  w.update(.05f,{});w.update(.05f,jump); assert(w.hero.air>0);
  // Glide consumes stamina while descending and has nonnegative altitude.
  jump.jump=false;jump.glide=true;
  float before=w.hero.stamina;for(int i=0;i<20;++i)w.update(.05f,jump);
  assert(w.hero.stamina<=before && w.hero.air>=0.f);
  // Day/night lighting is bounded, periodic and starts in daylight.
  w.reset();assert(w.daylight()>.99f);
  w.time=120.f;assert(w.daylight()<.01f);
  w.time=240.f;assert(w.daylight()>.99f);
  // Decorative tree positions and physical tree blockers must match.
  w.reset();int checkedTrees=0;
  for(int y=4;y<World::H-4;++y)for(int x=4;x<World::W-4;++x) {
    const auto tree=treeAt(w,x,y);
    if(tree.exists){
      assert(w.blocked(tree.x,tree.y));
      ++checkedTrees;
    }
  }
  assert(checkedTrees>40);
  // 0.2 saves can overlap a new 0.3 tree; they must be relocated on load.
  for(int y=4;y<World::H-4;++y) {
    bool finished=false;
    for(int x=4;x<World::W-4;++x) {
      const auto tree=treeAt(w,x,y);
      if(!tree.exists)continue;
      const auto legacy=std::filesystem::temp_directory_path()/"aetheria-stage3-legacy.save";
      World legacyWorld;
      legacyWorld.hero.pos={tree.x,tree.y};
      assert(legacyWorld.save(legacy.string()));
      World migrated;
      assert(migrated.load(legacy.string()));
      assert(!migrated.blocked(migrated.hero.pos.x,migrated.hero.pos.y));
      assert(distance(migrated.hero.pos,legacyWorld.hero.pos)<=2.01f);
      std::filesystem::remove(legacy);
      finished=true;break;
    }
    if(finished)break;
  }
  // Check both obstacle sweeping and slope restrictions with a sampled gradient.
  bool steepFound=false;
  for(int y=5;y<World::H-5&&!steepFound;++y)
    for(int x=5;x<World::W-5&&!steepFound;++x) {
      Vec2 a{float(x)+.2f,float(y)+.2f};
      const Vec2 b{a.x+.1f,a.y};
      const float gradient=(w.height(b.x,b.y)-w.height(a.x,a.y))/.1f;
      if(gradient>.181f&&!w.blocked(a.x,a.y)&&!w.blocked(b.x,b.y)) {
        assert(!w.canStep(a,b));steepFound=true;
      }
    }
  assert(steepFound);
  // Animation phase tracks displacement, not idle time.
  w.reset();for(auto& e:w.enemies)e.alive=false;
  const float initialStride=w.hero.stridePhase;
  Input walking;walking.right=true;w.update(.05f,walking);
  assert(w.hero.stridePhase>initialStride&&w.hero.motion>0);
  const float finalStride=w.hero.stridePhase;
  w.update(.05f,{});
  assert(w.hero.stridePhase==finalStride&&w.hero.motion==0);
  assert(!w.canStep({40,42},{40,46})); // swept motion has a bounded step size
  std::cout<<"All Aetheria stage 3 model tests passed ("<<checkedTrees<<" trees)\n";
}
