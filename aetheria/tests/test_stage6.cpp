#include "Model.h"
#include "GameplayAssist.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace aetheria;
using namespace legends;
static void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
static bool near(float a,float b,float tolerance=.015f){
 return std::abs(a-b)<tolerance;
}
int main(){
 try{
  World w(20261110);
  check(!w.crystals.empty()&&!w.enemies.empty()&&!w.npcs.empty(),"real 0.5 quest data");
  auto goal=nearestQuestTarget(w);
  check(goal.valid()&&goal.kind==GoalKind::Crystal,"find next missing crystal");
  float best=1e6f;
  for(const auto& crystal:w.crystals)if(!crystal.taken)
    best=std::min(best,distance(crystal.pos,w.hero.pos));
  check(near(goal.distance,best),"nearest crystal selected");
  const float bearing=directionToTarget(w.hero.pos,goal.point,0.f);
  check(std::isfinite(bearing)&&std::abs(bearing)<=3.142f,"finite compass direction");
  check(near(directionToTarget({0,0},{10,0},0.f),0.f),"east is camera forward");
  check(near(directionToTarget({0,0},{0,10},0.f),1.5707963f),
        "north relative angle uses world coordinates");
  for(auto& c:w.crystals)c.taken=true;
  w.collected=5;
  goal=nearestQuestTarget(w);
  check(goal.kind==GoalKind::Guard,"second chapter seeks live guard");
  for(auto& e:w.enemies)e.alive=false;
  w.defeated=4;
  goal=nearestQuestTarget(w);
  check(goal.kind==GoalKind::Shrine&&near(goal.distance,distance(w.hero.pos,w.shrine)),
        "final chapter seeks shrine");
  w.won=true;
  check(!nearestQuestTarget(w).valid(),"no navigation after victory");
  w.won=false;w.lost=true;
  check(!nearestQuestTarget(w).valid(),"no navigation after defeat");
  w.lost=false;w.collected=0;
  w.crystals.clear();
  w.sideQuest=0;
  check(nearestQuestTarget(w).kind==GoalKind::Elder,"side quest fallback");
  w.sideQuest=1;
  check(nearestQuestTarget(w).kind==GoalKind::Herb,"find healing herbs when needed");
  w.sideQuest=3;
  check(!nearestQuestTarget(w).valid(),"completed side quest has no marker");

  CameraControl camera;
  check(near(camera.zoom(),1.f),"original camera framing");
  camera.wheel(1);
  check(camera.zoom()<1.f,"wheel up zoom in");
  camera.wheel(-1);
  check(near(camera.zoom(),1.f),"wheel reverses");
  for(int i=0;i<100;++i)camera.wheel(5);
  check(near(camera.zoom(),.70f),"zoom in clamped");
  for(int i=0;i<100;++i)camera.wheel(-5);
  check(near(camera.zoom(),1.65f),"zoom out clamped");
  check(near(CameraControl::dragYaw(0.f,100.f),.75f),"right drag rotates");
  check(std::isfinite(CameraControl::dragYaw(0.f,1e9f)),"huge drag bounded");

  AutoSaveClock save;
  for(int i=0;i<1799;++i)check(!save.advance(.05f,true),"not due too early");
  check(save.advance(.05f,true),"save after 90 active seconds");
  save.complete(true);
  check(near(save.elapsed(),0.f),"successful save resets timer");
  for(int i=0;i<100;++i)save.advance(.05f,false);
  check(near(save.elapsed(),0.f),"paused time does not count");
  for(int i=0;i<1800;++i)save.advance(.05f,true);
  save.complete(false);
  check(near(save.elapsed(),75.f),"failed save retries after 15 active seconds");
  for(int i=0;i<299;++i)check(!save.advance(.05f,true),"retry delayed");
  check(save.advance(.05f,true),"retry after deadline");
  save.reset();check(near(save.elapsed(),0.f),"loading resets timer");

  // Autosaving invokes the original model writer; old save layout stays intact.
  World original(20261110); original.hero.level=4;original.hero.potions=8;
  const auto path=std::filesystem::temp_directory_path()/"aetheria-stage6-compat.save";
  check(original.save(path.string()),"write unchanged save format");
  World restored(20261110);
  check(restored.load(path.string()),"read autosaved file");
  check(restored.hero.level==4&&restored.hero.potions==8,"progress preserved");
  std::filesystem::remove(path);
  std::cout<<"Aetheria 0.6 recovery: navigation, zoom, input, active-only autosave, save compatibility PASS\n";
 }catch(const std::exception& e){
  std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
 }
}
