#include "Model.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
using namespace legends;
int main(){
  World w(20261110);
  assert(w.hero.weapon==0 && w.hero.charm==0);
  assert(w.crystalShards==0 && w.essence==0);
  // Non-modal choices must not force the player to accept an offer.
  w.hero.pos=w.npcs[0].pos;
  assert(w.interact());
  assert(w.dialogueChoices(true).size()==2);
  assert(w.chooseDialogue(1));
  assert(w.activeNpc==-1 && w.sideQuest==0);
  assert(w.interact() && w.chooseDialogue(0) && w.dialoguePage==1);
  assert(w.chooseDialogue(1) && w.sideQuest==0);
  assert(w.interact() && w.chooseDialogue(0) && w.chooseDialogue(0));
  assert(w.sideQuest==1 && w.activeNpc==-1);
  assert(w.interact() && w.chooseDialogue(0) && w.dialoguePage==2);
  assert(w.dialogueText(true).find("BLOMMORNA")!=std::string::npos);
  assert(w.chooseDialogue(0) && w.activeNpc==-1);
  w.hero.hp=40.f;
  w.hero.pos=w.npcs[1].pos;
  assert(w.interact() && w.chooseDialogue(1) && !w.healerRested);
  assert(w.interact() && w.chooseDialogue(0) && w.chooseDialogue(0));
  assert(w.healerRested && w.hero.hp==85.f);

  // Crafted items have finite costs and cannot be crafted repeatedly.
  w.crystalShards=3;w.essence=5;
  assert(!w.craft(4));
  assert(w.craft(1));
  assert(w.hero.weapon==1 && w.crystalShards==1 && w.essence==3);
  assert(!w.craft(1));
  assert(w.craft(2));
  assert(w.hero.charm==1 && w.crystalShards==0 && w.essence==0);
  assert(!w.craft(2));
  // Gear has real gameplay effects, not just cosmetic changes.
  World plain, upgraded;
  plain.enemies.resize(1);upgraded.enemies.resize(1);
  plain.enemies[0].pos={41,42};upgraded.enemies[0].pos={41,42};
  plain.hero.facing=upgraded.hero.facing=0;
  upgraded.hero.weapon=1;
  assert(plain.attack() && upgraded.attack());
  assert(std::fabs((plain.enemies[0].hp-upgraded.enemies[0].hp)-18.f)<.0001f);
  plain.hero.stamina=upgraded.hero.stamina=50;
  upgraded.hero.charm=1;
  plain.update(.05f,{});upgraded.update(.05f,{});
  assert(upgraded.hero.stamina>plain.hero.stamina);
  // Materials increase only once on unique pickups.
  World drops;
  drops.enemies.clear();
  drops.hero.pos=drops.crystals[0].pos;
  drops.update(.01f,{});
  assert(drops.collected==1 && drops.crystalShards==1);
  drops.update(.01f,{});
  assert(drops.collected==1 && drops.crystalShards==1);
  World kill;
  kill.enemies.clear();
  Enemy enemy;enemy.pos=enemy.home={41,42};enemy.hp=1;
  kill.enemies.push_back(enemy);kill.hero.facing=0;
  assert(kill.attack() && kill.essence==1 && kill.defeated==1);
  assert(kill.essence==1 && !kill.enemies[0].alive);

  // Save, load, and validate a two-extension 0.5 save.
  const auto tmp=std::filesystem::temp_directory_path()/"aetheria-stage5-test.save";
  assert(w.save(tmp.string()));
  World readBack;
  assert(readBack.load(tmp.string()));
  assert(readBack.hero.weapon==1 && readBack.hero.charm==1);
  assert(readBack.crystalShards==0 && readBack.essence==0);
  assert(readBack.sideQuest==1 && readBack.healerRested);
  std::ifstream saved(tmp);const std::string data((std::istreambuf_iterator<char>(saved)),{});
  const auto ext=data.find("AETHERIA_EXTRA_5");
  assert(ext!=std::string::npos);
  {std::ofstream o(tmp,std::ios::trunc);o<<data.substr(0,ext);}
  World old;
  assert(old.load(tmp.string()));
  assert(old.hero.weapon==0 && old.hero.charm==0);
  // Legacy migration restores earned loot, not free starter gear.
  assert(old.crystalShards==old.collected && old.essence==old.defeated);
  const auto before=readBack.hero.weapon;
  {std::ofstream o(tmp,std::ios::trunc);o<<data.substr(0,ext)<<"AETHERIA_EXTRA_5 999 0 1 1\n";}
  assert(!readBack.load(tmp.string()) && readBack.hero.weapon==before);
  {std::ofstream o(tmp,std::ios::trunc);o<<data<<"INJECTED TRAILING CONTENT\n";}
  assert(!readBack.load(tmp.string()));
  std::filesystem::remove(tmp);
  std::cout<<"Aetheria stage 5: inventory, craft, equipment, dialogue choices, save compatibility passed\n";
}
