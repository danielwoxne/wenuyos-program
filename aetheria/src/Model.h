#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace legends {
struct Vec2 { float x=0, y=0; };
enum class Element { Fire=0, Frost=1, Wind=2, Thunder=3 };
enum class Tile { Grass, Forest, Sand, Stone, Water, Path };
enum class EnemyState { Patrol, Chase, Windup, Recover, Return };
struct Enemy {
  Vec2 pos; float hp=70, slow=0, attackTimer=0; bool alive=true;
  // 0.4 simulation state. Old save files still contain only the original fields.
  Vec2 home{}; EnemyState state=EnemyState::Patrol;
  float windup=0, alert=0, stagger=0, burn=0, hurtPulse=0, phase=0;
};
struct Crystal { Vec2 pos; bool taken=false; };
struct Herb { Vec2 pos; bool taken=false; };
struct NPC {
  Vec2 pos,home; int kind=0; float facing=0,phase=0,motion=0,stridePhase=0;
};
struct Input { bool up=false,down=false,left=false,right=false,sprint=false,attack=false,jump=false,glide=false;
  bool analog=false; float moveX=0,moveY=0; };
struct Player {
  Vec2 pos{40,42}; float hp=100, stamina=100, attackCooldown=0, skillCooldown=0;
  float air=0, verticalSpeed=0;
  // Animation state is transient and deliberately not part of the legacy save.
  float facing=-0.785398f, stridePhase=0, motion=0;
  bool jumpLatched=false, gliding=false;
  int level=1, xp=0, potions=3;
  // Two crafted equipment slots; 0 means starter gear / unequipped.
  int weapon=0, charm=0;
  Element element=Element::Fire;
};
class World {
public:
  static constexpr int W=80,H=80;
  Player hero;
  std::vector<Enemy> enemies;
  std::vector<Crystal> crystals;
  std::vector<Herb> herbs;
  std::vector<NPC> npcs;
  int crystalShards=0, essence=0; // materials are separate from quest counters
  int herbsCollected=0, sideQuest=0; // 0 available, 1 collecting, 2 return, 3 rewarded
  int activeNpc=-1, dialoguePage=0; // transient, never written to save
  bool healerRested=false;
  Vec2 shrine{42,40};
  int defeated=0, collected=0;
  bool won=false, lost=false, paused=false;
  float time=0, strikePulse=0, skillPulse=0, skillRadius=0;
  Vec2 pulseAt{};
  std::string message;
  float messageSeconds=0;
  explicit World(uint32_t seed=20261110);
  void reset(uint32_t seed=20261110);
  Tile tile(int x,int y) const;
  float height(float x,float y) const; // continuous procedural elevation for 3D mesh
  bool blocked(float x,float y) const;
  bool canStep(Vec2 from,Vec2 to) const; // inclines and obstacle sweep
  float daylight() const; // 0 = night; 1 = bright noon
  void update(float dt, const Input& input);
  bool attack();
  bool skill();
  bool potion();
  void cycleElement();
  bool interact();
  bool chooseDialogue(int choice); // 0 / 1; E chooses the first option
  std::vector<std::string> dialogueChoices(bool swedish=false) const;
  bool craft(int recipe); // 1: rune sword; 2: forest charm
  void closeDialogue();
  std::string dialogueText(bool swedish=false) const;
  std::string dialogueSpeaker(bool swedish=false) const;
  int nearestNpc(float radius=2.0f) const;
  int questStage() const; // 0 crystals; 1 enemies; 2 shrine; 3 complete
  bool save(const std::string& path) const;
  bool load(const std::string& path);
  uint32_t seed() const { return seed_; }
private:
  uint32_t seed_=20261110;
  void hitEnemy(Enemy& e,float amount);
  void gainExperience(int amount);
  void moveEnemy(Enemy& e,Vec2 target,float speed,float dt);
  void say(std::string value);
};
float distance(Vec2 a,Vec2 b);
}
