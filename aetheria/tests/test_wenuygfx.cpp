#include "Scene3D.h"
#include "Model.h"
#include <wenuygfx/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
int main(){
 try{
  legends::World world(20261110);
  const auto originalPlayer=world.hero.pos;
  aetheria::Scene3D adapter;
  wenuygfx::Scene worldScene;
  wenuygfx::Camera camera;
  auto stats=adapter.collectWenuyGfx(world,-.785398f,true,960,600,worldScene,camera);
  check(stats.triangles>250 && stats.triangles<=26000,"bounded shared world triangles");
  check(stats.terrainTiles>100,"actual Aetheria terrain used");
  check(world.hero.pos.x==originalPlayer.x && world.hero.pos.y==originalPlayer.y,
        "building render scene never alters game model");
  for(const auto& t:worldScene.triangles)
    for(float n:{t.a.x,t.a.y,t.a.z,t.b.x,t.b.y,t.b.z,t.c.x,t.c.y,t.c.z})
      check(std::isfinite(n),"all exported vertices finite");
  wenuygfx::SoftwareRenderer cpu;
  wenuygfx::Frame frame;
  check(cpu.render(worldScene,camera,480,300,frame),"WenuyGFX CPU renderer");
  check(frame.width==480&&frame.height==300&&frame.argb.size()==480u*300u,
        "low frame memory bounded");
  const auto first=frame.argb[frame.argb.size()/2];
  check((first&0xff000000u)==0xff000000u,"opaque output");
  const size_t oldCount=worldScene.triangles.size();
  auto next=adapter.collectWenuyGfx(world,-.785398f,true,960,600,worldScene,camera);
  check(next.triangles==static_cast<int>(oldCount),"repeated export deterministic size");
  auto changed=adapter.collectWenuyGfx(world,.2f,true,960,600,worldScene,camera);
  check(changed.triangles>100,"camera rotation still produces scene");
  (void)adapter.collectWenuyGfx(world,0,true,-1,600,worldScene,camera);
  std::cout<<"Aetheria real-world geometry -> WenuyGFX 0.9 CPU (low/soft) PASS\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
