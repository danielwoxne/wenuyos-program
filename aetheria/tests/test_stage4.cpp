#include "Model.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
using namespace legends;
static void noEnemies(World& w) {for(auto& e:w.enemies)e.alive=false;w.defeated=static_cast<int>(w.enemies.size());}
int main() {
  World w(2345);
  assert(w.npcs.size()==2 && w.herbs.size()==6 && w.sideQuest==0);
  assert(w.herbsCollected==0 && w.activeNpc==-1);
  assert(!w.dialogueText().size());
  assert(w.nearestNpc()==-1);
  noEnemies(w);
  w.hero.pos=w.npcs[0].pos;
  assert(w.interact() && w.activeNpc==0);
  assert(w.dialogueText(true).find("SKOGENS")!=std::string::npos);
  assert(w.dialogueSpeaker()=="ELDER ARIN");
  // Movement / combat is suspended while reading a dialogue.
  auto start=w.hero.pos;
  Input moving;moving.right=true;moving.attack=true;
  w.update(.05f,moving);
  assert(distance(w.hero.pos,start)<.0001f && !w.attack() && !w.skill());
  assert(w.interact() && w.dialoguePage==1);
  assert(w.interact() && w.activeNpc==-1 && w.sideQuest==1);
  // Harvest exactly three; quest should change to a turn-in state.
  for(int i=0;i<3;++i){w.hero.pos=w.herbs[i].pos;w.update(.01f,{});}
  assert(w.herbsCollected==3 && w.sideQuest==2);
  const int potions=w.hero.potions;
  w.hero.pos=w.npcs[0].pos;assert(w.interact());
  assert(w.activeNpc==0 && w.interact());
  assert(w.sideQuest==3 && w.hero.potions==potions+2 && w.hero.xp>=35);
  assert(w.activeNpc==-1);
  // One-time healer, no unlimited regeneration exploit.
  w.hero.hp=33;w.hero.pos=w.npcs[1].pos;
  assert(w.interact());assert(w.interact());assert(w.interact());
  assert(w.healerRested && w.hero.hp==78);
  w.hero.hp=20;assert(w.interact());assert(w.interact());
  assert(w.hero.hp==20);
  w.hero.pos=w.npcs[0].pos;assert(w.interact());w.closeDialogue();assert(w.activeNpc==-1);

  // Save v4 extension, restore without mutating invalid inputs.
  const auto saved=std::filesystem::temp_directory_path()/"aetheria-stage4.save";
  assert(w.save(saved.string()));
  World fromSave;assert(fromSave.load(saved.string()));
  assert(fromSave.sideQuest==3 && fromSave.herbsCollected==3 && fromSave.healerRested);
  assert(fromSave.activeNpc==-1 && fromSave.hero.hp==20);
  // 0.1 through 0.3 formats have no optional extension.
  std::ifstream in(saved);std::string content((std::istreambuf_iterator<char>(in)),{});
  const auto extension=content.find("AETHERIA_EXTRA_4");assert(extension!=std::string::npos);
  const auto legacy=std::filesystem::temp_directory_path()/"aetheria-stage3-old.save";
  {std::ofstream out(legacy);out<<content.substr(0,extension);}
  World fromLegacy;assert(fromLegacy.load(legacy.string()));
  assert(fromLegacy.sideQuest==0 && fromLegacy.herbsCollected==0 && !fromLegacy.healerRested);
  {std::ofstream out(saved,std::ios::trunc);out<<content.substr(0,extension)<<"AETHERIA_EXTRA_4 3 99 1\n";}
  assert(!fromSave.load(saved.string()) && fromSave.sideQuest==3);
  std::filesystem::remove(saved);std::filesystem::remove(legacy);

  // Enemies must telegraph before a hit, and dodging should cancel damage.
  w.reset();w.enemies.clear();
  Enemy e;e.pos=e.home={40.9f,42.f};e.hp=90;
  w.enemies.push_back(e);const float hp=w.hero.hp;
  w.update(.05f,{});
  assert(w.enemies[0].windup>0 && w.enemies[0].state==EnemyState::Windup);
  assert(w.hero.hp==hp);
  w.hero.pos={40.f,44.f};for(int i=0;i<20;++i)w.update(.05f,{});
  assert(w.hero.hp==hp && w.enemies[0].attackTimer>0);
  // Enemy normally returns when the player is gone.
  w.reset();w.enemies.clear();
  Enemy away;away.home={33.f,37.f};away.pos={36.f,37.f};w.enemies.push_back(away);
  w.hero.pos={7.f,7.f};w.update(.05f,{});
  assert(w.enemies[0].state==EnemyState::Return);
  // Fire inflicts an explicit burning status, not merely a color change.
  w.reset();w.enemies.clear();
  Enemy burning;burning.home=burning.pos={41.f,42.f};burning.hp=200;
  w.enemies.push_back(burning);
  assert(w.skill() && w.enemies[0].burn>0);
  const float afterSkill=w.enemies[0].hp;
  w.hero.pos={40.f,45.f};w.update(.05f,{});
  assert(w.enemies[0].hp<afterSkill);
  std::cout<<"Aetheria stage 4: NPCs, quests, dialogue, legacy saves and AI passed\n";
}
