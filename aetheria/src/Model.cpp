#include "Model.h"
#include "Environment.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace legends {
namespace {
constexpr float kPi = 3.14159265f;
float clampf(float v,float low,float high) { return std::max(low,std::min(high,v)); }
uint32_t hash(uint32_t a) {
  a ^= a>>16; a*=0x7feb352du; a ^= a>>15; a*=0x846ca68bu; a ^= a>>16;
  return a;
}
float noise(int x,int y,uint32_t seed) {
  return static_cast<float>(hash(static_cast<uint32_t>(x)*374761393u ^
       static_cast<uint32_t>(y)*668265263u ^ seed)%10000)/10000.f;
}
} // namespace
float distance(Vec2 a,Vec2 b) { return std::hypot(a.x-b.x,a.y-b.y); }
World::World(uint32_t seed) { reset(seed); }
void World::reset(uint32_t seed) {
  seed_=seed; hero=Player{}; time=0; defeated=0; collected=0; won=lost=paused=false;
  strikePulse=skillPulse=0; messageSeconds=0; pulseAt=hero.pos; skillRadius=0;
  crystals.clear(); enemies.clear(); herbs.clear(); npcs.clear();
  crystalShards=essence=0;
  herbsCollected=0; sideQuest=0; activeNpc=-1; dialoguePage=0; healerRested=false;
  const Vec2 places[]={{35,39},{47,40},{40,34},{34,48},{48,49},{28,38},{54,44},{42,54},{51,33}};
  for(auto p:places) crystals.push_back({p,false});
  const Vec2 mobs[]={{36,36},{46,36},{32,42},{51,43},{37,50},{47,53},{28,45},{54,36},
                     {26,35},{58,43},{50,29},{40,58},{59,52},{23,50}};
  for(auto p:mobs) {
    Enemy e; e.pos=e.home=p;
    e.hp=65.f+static_cast<float>(enemies.size()%3)*12.f;
    e.phase=static_cast<float>(enemies.size())*.91f;
    enemies.push_back(e);
  }
  for(auto p:std::vector<Vec2>{{39,48},{47,46},{36,45},{45,52},{34,49},{50,50}})
    herbs.push_back({p,false});
  for(const auto& entry:std::vector<std::pair<Vec2,int>>{{{40,45},0},{{46,44},1}}) {
    NPC n; n.home=n.pos=entry.first; n.kind=entry.second; n.phase=float(entry.second)*2.6f;
    npcs.push_back(n);
  }
  say("FIND THE CRYSTALS");
}
Tile World::tile(int x,int y) const {
  if(x<0||y<0||x>=W||y>=H) return Tile::Water;
  if(std::abs(x-42)<=2 && std::abs(y-40)<=2) return Tile::Path;
  if((x>=39 && x<=43 && y>=40 && y<=46) || (y==40 && x>=32 && x<=52)) return Tile::Path;
  const float r=std::hypot(static_cast<float>(x-40),static_cast<float>(y-40));
  const float wave=std::sin(x*0.19f)+std::cos(y*0.17f)+std::sin((x+y)*0.12f);
  const float n=noise(x/3,y/3,seed_);
  if(r>19 && (wave+2*n < -0.30f || x<5||y<5||x>=75||y>=75)) return Tile::Water;
  if(r>16 && wave>2.45f) return Tile::Stone;
  if(r>18 && wave < 0.45f) return Tile::Sand;
  if(n>0.64f && r>5) return Tile::Forest;
  return Tile::Grass;
}
float World::height(float x,float y) const {
  // Smooth and deterministic: adjacent mesh vertices share the same elevation.
  const float swell=.57f*std::sin(x*.195f)+.43f*std::cos(y*.167f)
    +.30f*std::sin((x+y)*.13f)+.16f*std::cos((x-y)*.34f);
  return std::max(.16f,1.05f+swell);
}
float World::daylight() const {
  // 4-minute cycle. Starts in daylight to keep the opening area easy to navigate.
  constexpr float cycle=240.f;
  return clampf(.5f+.5f*std::cos(time*(2.f*kPi/cycle)),0.f,1.f);
}
bool World::blocked(float x,float y) const {
  if(!std::isfinite(x)||!std::isfinite(y)||x<1||y<1||x>=W-1||y>=H-1)return true;
  constexpr float radius=.22f;
  for(const float ox:{-radius,radius})for(const float oy:{-radius,radius}) {
    float tx=x+ox,ty=y+oy;
    if(tile(static_cast<int>(tx),static_cast<int>(ty))==Tile::Water)return true;
  }
  // Only nearby cells are sampled. The same tree placement drives rendering.
  const int cx=static_cast<int>(std::floor(x)),cy=static_cast<int>(std::floor(y));
  for(int j=cy-1;j<=cy+1;++j)for(int i=cx-1;i<=cx+1;++i) {
    const TreePosition tree=treeAt(*this,i,j);
    if(tree.exists && std::hypot(x-tree.x,y-tree.y)<(.19f*tree.scale+radius))return true;
  }
  return false;
}
bool World::canStep(Vec2 from,Vec2 to) const {
  if(!std::isfinite(from.x)||!std::isfinite(from.y)||
     !std::isfinite(to.x)||!std::isfinite(to.y))return false;
  const float length=distance(from,to);
  if(length>3.f)return false; // teleports must not use this function
  const int steps=std::max(1,static_cast<int>(std::ceil(length/.12f)));
  Vec2 prev=from;
  for(int i=1;i<=steps;++i) {
    float f=static_cast<float>(i)/steps;
    Vec2 next{from.x+(to.x-from.x)*f,from.y+(to.y-from.y)*f};
    if(blocked(next.x,next.y))return false;
    const float elevation=height(next.x,next.y)-height(prev.x,prev.y);
    const float horizontal=distance(next,prev);
    // A steep upward incline must be approached via a gentler route.
    if(horizontal>.00001f && elevation/horizontal>.18f)return false;
    prev=next;
  }
  return true;
}
void World::say(std::string value) { message=std::move(value); messageSeconds=2.8f; }
int World::questStage() const { return won?3:collected<5?0:defeated<4?1:2; }
void World::gainExperience(int amount) {
  hero.xp+=amount;
  while(hero.level<100 && hero.xp>=hero.level*60) {
    hero.xp-=hero.level*60;
    ++hero.level;
    hero.hp=std::min(100.f,hero.hp+20.f);
    say("LEVEL UP!");
  }
}
void World::hitEnemy(Enemy& e,float amount) {
  if(!e.alive) return;
  e.hp -= amount; e.hurtPulse=.28f; e.stagger=.18f;
  if(e.hp<=0) {
    e.hp=0; e.alive=false; ++defeated; essence=std::min(99,essence+1); gainExperience(25);
  }
}
bool World::attack() {
  if(won||lost||paused||activeNpc>=0||hero.attackCooldown>0) return false;
  hero.attackCooldown=.42f; strikePulse=.26f; pulseAt=hero.pos;
  Enemy* closest=nullptr; float best=2.25f;
  for(auto& e:enemies) if(e.alive) {
    float d=distance(hero.pos,e.pos);
    if(d<best) {
      const float front=std::cos(hero.facing)*(e.pos.x-hero.pos.x)+
                        std::sin(hero.facing)*(e.pos.y-hero.pos.y);
      if(d<.85f || front>=-.1f*d) {closest=&e; best=d;}
    }
  }
  if(closest) hitEnemy(*closest,34.f+hero.level*5.f+(hero.weapon?18.f:0.f));
  return true;
}
bool World::skill() {
  if(won||lost||paused||activeNpc>=0||hero.skillCooldown>0) return false;
  hero.skillCooldown=5.0f; skillPulse=.55f; skillRadius=3.3f; pulseAt=hero.pos;
  for(auto& e:enemies) if(e.alive) {
    float d=distance(hero.pos,e.pos);
    if(d>skillRadius) continue;
    switch(hero.element) {
      case Element::Fire: hitEnemy(e,52); if(e.alive)e.burn=3.f; break;
      case Element::Frost: hitEnemy(e,37); e.slow=3.6f; break;
      case Element::Wind: {
        hitEnemy(e,44); float angle=std::atan2(e.pos.y-hero.pos.y,e.pos.x-hero.pos.x);
        float nx=e.pos.x+std::cos(angle)*1.2f, ny=e.pos.y+std::sin(angle)*1.2f;
        if(!blocked(nx,ny)) e.pos={nx,ny};
        break;
      }
      case Element::Thunder: hitEnemy(e,54); break;
    }
  }
  return true;
}
bool World::potion() {
  if(paused||won||lost||activeNpc>=0||hero.potions<1||hero.hp>=100) return false;
  --hero.potions; hero.hp=std::min(100.f,hero.hp+45.f); say("RESTORED HEALTH"); return true;
}
void World::cycleElement() {
  if(!won&&!lost&&activeNpc<0) hero.element=static_cast<Element>((static_cast<int>(hero.element)+1)%4);
}
int World::nearestNpc(float radius) const {
  int nearest=-1;float best=radius;
  for(size_t i=0;i<npcs.size();++i) {
    float d=distance(hero.pos,npcs[i].pos);
    if(d<best) {best=d;nearest=static_cast<int>(i);}
  }
  return nearest;
}
void World::closeDialogue() {activeNpc=-1;dialoguePage=0;}
std::string World::dialogueSpeaker(bool swedish) const {
  if(activeNpc<0||activeNpc>=static_cast<int>(npcs.size()))return "";
  return npcs[activeNpc].kind==0?(swedish?"ÄLDSTE ARIN":"ELDER ARIN"):
                                   (swedish?"LÄKAREN SERA":"HEALER SERA");
}
std::string World::dialogueText(bool sv) const {
  if(activeNpc<0||activeNpc>=static_cast<int>(npcs.size()))return "";
  if(npcs[activeNpc].kind==1) {
    if(healerRested) return sv?"VAR RÄDD OM DIG PÅ VÄGARNA.":"TRAVEL SAFELY, MY FRIEND.";
    return dialoguePage==0?(sv?"DU SER TRÖTT UT. FÅR JAG HJÄLPA DIG?":
                                 "YOU LOOK TIRED. MAY I HELP?"):
                           (sv?"JAG KAN LÄKA DINA SÅR EN GÅNG.":
                               "I CAN TEND YOUR WOUNDS ONCE.");
  }
  if(sideQuest==0)
    return dialoguePage==0?(sv?"SKOGENS LJUS HAR BÖRJAT BLEKNA.":
                                 "THE FOREST LIGHT IS FADING."):
                           (sv?"HÄMTA TRE LÄKEÖRTER TILL BYN.":
                               "PLEASE FIND THREE HEALING HERBS.");
  if(sideQuest==1)
    return dialoguePage==2?(sv?"TITTA BLAND DE GRÖNA BLOMMORNA PÅ ÄNGARNA.":"LOOK FOR THE GREEN FLOWERS IN THE MEADOWS."):
      sv?"VI BEHÖVER TRE ÖRTER. SÖK PÅ ÄNGARNA.":
              "WE STILL NEED THREE HERBS FROM THE MEADOWS.";
  if(sideQuest==2)
    return sv?"UTMÄRKT! TA DESSA DRYCKER SOM TACK.":
              "WONDERFUL! PLEASE TAKE THESE POTIONS.";
  return sv?"TACK VARE DIG MÅR BYN BÄTTRE.":
            "OUR VILLAGE IS SAFER THANKS TO YOU.";
}
std::vector<std::string> World::dialogueChoices(bool sv) const {
  if(activeNpc<0 || activeNpc>=static_cast<int>(npcs.size()))return {};
  const bool elder=npcs[activeNpc].kind==0;
  if(elder){
    if(sideQuest==0){
      if(dialoguePage==0) return {sv?"1  BERÄTTA MER":"1  TELL ME MORE",sv?"2  HEJDÅ":"2  GOODBYE"};
      return {sv?"1  JAG HJÄLPER ER":"1  I'LL HELP",sv?"2  INTE NU":"2  NOT NOW"};
    }
    if(sideQuest==1){
      if(dialoguePage==2)return {sv?"1  TACK FÖR TIPSET":"1  THANKS FOR THE TIP",sv?"2  HEJDÅ":"2  GOODBYE"};
      return {sv?"1  VAR FINNS ÖRTERNA?":"1  WHERE ARE THE HERBS?",sv?"2  HEJDÅ":"2  GOODBYE"};
    }
    if(sideQuest==2)return {sv?"1  LÄMNA ÖRTER":"1  RETURN HERBS",sv?"2  SENARE":"2  LATER"};
    return {sv?"1  TACK!":"1  THANK YOU",sv?"2  HEJDÅ":"2  GOODBYE"};
  }
  if(healerRested)return {sv?"1  FÅ ETT RÅD":"1  ASK FOR ADVICE",sv?"2  HEJDÅ":"2  GOODBYE"};
  if(dialoguePage==0)return {sv?"1  KAN DU HJÄLPA?":"1  CAN YOU HELP?",sv?"2  NEJ TACK":"2  NO THANKS"};
  return {sv?"1  LÄK MINA SÅR":"1  HEAL MY WOUNDS",sv?"2  SENARE":"2  LATER"};
}
bool World::chooseDialogue(int choice) {
  if(activeNpc<0 || activeNpc>=static_cast<int>(npcs.size())||(choice!=0&&choice!=1))return false;
  if(choice==1) {closeDialogue();return true;}
  const bool elder=npcs[activeNpc].kind==0;
  if(elder && sideQuest==0 && dialoguePage==0){dialoguePage=1;return true;}
  if(!elder && !healerRested && dialoguePage==0){dialoguePage=1;return true;}
  if(elder){
    if(sideQuest==0){sideQuest=herbsCollected>=3?2:1;say("HERB QUEST STARTED");}
    else if(sideQuest==1){if(dialoguePage!=2){dialoguePage=2;return true;}}
    else if(sideQuest==2){sideQuest=3;hero.potions=std::min(99,hero.potions+2);gainExperience(35);say("SIDE QUEST COMPLETE");}
  }else if(!healerRested){healerRested=true;hero.hp=std::min(hero.hp+45.f,100.f);say("HEALER HELPED");}
  closeDialogue();return true;
}
bool World::craft(int recipe) {
  if(lost || won || activeNpc>=0)return false;
  if(recipe==1 && hero.weapon==0 && crystalShards>=2 && essence>=2){
    crystalShards-=2;essence-=2;hero.weapon=1;say("RUNE SWORD CRAFTED");return true;
  }
  if(recipe==2 && hero.charm==0 && crystalShards>=1 && essence>=3){
    crystalShards-=1;essence-=3;hero.charm=1;say("FOREST CHARM CRAFTED");return true;
  }
  say("NOT ENOUGH MATERIALS");return false;
}
bool World::interact() {
  if(lost||paused)return false;
  if(activeNpc>=0)return chooseDialogue(0);
  if(questStage()==2 && distance(hero.pos,shrine)<2.3f) {
    won=true; say("THE VALLEY IS FREE!"); return true;
  }
  const int near=nearestNpc();
  if(near>=0) {activeNpc=near;dialoguePage=0;return true;}
  if(distance(hero.pos,shrine)<2.3f) say("THE SHRINE NEEDS MORE POWER");
  return false;
}
void World::moveEnemy(Enemy& e,Vec2 target,float speed,float dt) {
  const float d=distance(e.pos,target);
  if(d<.04f)return;
  const float length=std::min(speed*dt,d);
  const Vec2 to={e.pos.x+(target.x-e.pos.x)*length/d,
                 e.pos.y+(target.y-e.pos.y)*length/d};
  if(canStep(e.pos,{to.x,e.pos.y}))e.pos.x=to.x;
  if(canStep(e.pos,{e.pos.x,to.y}))e.pos.y=to.y;
}
void World::update(float dt, const Input& input) {
  if(paused||lost||won||activeNpc>=0) return;
  dt=clampf(dt,0,.05f); time+=dt;
  hero.attackCooldown=std::max(0.f,hero.attackCooldown-dt);
  hero.skillCooldown=std::max(0.f,hero.skillCooldown-dt);
  strikePulse=std::max(0.f,strikePulse-dt);
  skillPulse=std::max(0.f,skillPulse-dt);
  messageSeconds=std::max(0.f,messageSeconds-dt);
  float dx=input.analog?input.moveX:static_cast<float>(input.right)-static_cast<float>(input.left);
  float dy=input.analog?input.moveY:static_cast<float>(input.down)-static_cast<float>(input.up);
  float magnitude=std::hypot(dx,dy);
  if(magnitude>0) { dx/=magnitude;dy/=magnitude; }
  const bool sprint=input.sprint && hero.stamina>1 && magnitude>0;
  const float speed=sprint?6.0f:3.35f;
  if(sprint) hero.stamina=std::max(0.f,hero.stamina-26.f*dt);
  else hero.stamina=std::min(100.f,hero.stamina+(hero.charm?27.f:20.f)*dt);
  float nx=hero.pos.x+dx*speed*dt, ny=hero.pos.y+dy*speed*dt;
  Vec2 old=hero.pos;
  if(canStep(hero.pos,{nx,hero.pos.y})) hero.pos.x=nx;
  if(canStep(hero.pos,{hero.pos.x,ny})) hero.pos.y=ny;
  const float traveled=distance(old,hero.pos);
  hero.motion=traveled/std::max(dt,.0001f);
  if(traveled>.00001f) {
    hero.facing=std::atan2(hero.pos.y-old.y,hero.pos.x-old.x);
    hero.stridePhase+=traveled*5.8f;
  }
  if(input.jump && !hero.jumpLatched && hero.air==0) hero.verticalSpeed=5.8f;
  hero.jumpLatched=input.jump;
  hero.gliding=false;
  if(hero.air>0 || hero.verticalSpeed>0) {
    const float gravity=(input.glide && hero.verticalSpeed<0 && hero.stamina>0)?3.1f:14.2f;
    if(gravity<5) {
      hero.stamina=std::max(0.f,hero.stamina-13.f*dt);
      hero.gliding=true;
    }
    hero.air=std::max(0.f,hero.air+hero.verticalSpeed*dt);
    hero.verticalSpeed-=gravity*dt;
    if(hero.air<=0 && hero.verticalSpeed<0) hero.verticalSpeed=0;
  }
  if(input.attack) attack();
  for(auto& c:crystals) {
    if(!c.taken && distance(hero.pos,c.pos)<.85f) {
      c.taken=true; ++collected; crystalShards=std::min(99,crystalShards+1);
      gainExperience(6); say("CRYSTAL FOUND!");
    }
  }
  for(auto& herb:herbs) if(!herb.taken && distance(hero.pos,herb.pos)<.78f) {
    herb.taken=true; ++herbsCollected;
    if(sideQuest==1 && herbsCollected>=3) {sideQuest=2;say("RETURN TO ARIN");}
    else say("HEALING HERB FOUND");
  }
  for(auto& n:npcs) {
    const Vec2 start=n.pos;
    Vec2 target{n.home.x+std::cos(time*.34f+n.phase)*.70f,
                n.home.y+std::sin(time*.34f+n.phase)*.55f};
    const float d=distance(n.pos,target);
    if(d>.04f) {
      const float step=std::min(d,dt*.47f);
      Vec2 goal{n.pos.x+(target.x-n.pos.x)*step/d,
                n.pos.y+(target.y-n.pos.y)*step/d};
      if(canStep(n.pos,{goal.x,n.pos.y}))n.pos.x=goal.x;
      if(canStep(n.pos,{n.pos.x,goal.y}))n.pos.y=goal.y;
    }
    const float moved=distance(start,n.pos);
    n.motion=moved/std::max(.00001f,dt);
    if(moved>.00001f){n.facing=std::atan2(n.pos.y-start.y,n.pos.x-start.x);n.stridePhase+=moved*5.8f;}
  }
  for(auto& e:enemies) {
    if(!e.alive) continue;
    e.attackTimer=std::max(0.f,e.attackTimer-dt);
    e.slow=std::max(0.f,e.slow-dt);
    e.stagger=std::max(0.f,e.stagger-dt);
    e.hurtPulse=std::max(0.f,e.hurtPulse-dt);
    if(e.burn>0) {
      const float tick=std::min(dt,e.burn);e.burn=std::max(0.f,e.burn-dt);
      hitEnemy(e,8.f*tick);
      if(!e.alive)continue;
    }
    const float d=distance(e.pos,hero.pos);
    if(d<7.f) e.alert=3.5f;
    else e.alert=std::max(0.f,e.alert-dt);
    if(e.windup>0.f) {
      e.state=EnemyState::Windup;
      e.windup=std::max(0.f,e.windup-dt);
      if(e.windup<=0.f) {
        // A telegraphed hit is avoidable by moving out of range or jumping.
        if(distance(e.pos,hero.pos)<1.40f && hero.air<.25f) {
          hero.hp=std::max(0.f,hero.hp-(e.slow>0?5.f:9.f)*(hero.charm?.8f:1.f));
          if(hero.hp<=0){lost=true;say("THE JOURNEY ENDS HERE");}
        }
        e.attackTimer=1.4f;e.state=EnemyState::Recover;
      }
      continue;
    }
    if(e.stagger>0)continue;
    if(e.attackTimer>0 && e.state==EnemyState::Recover)continue;
    if(e.alert>0.f && d<11.5f) {
      e.state=EnemyState::Chase;
      if(d<1.45f && e.attackTimer==0.f) {
        e.windup=e.slow>0?.82f:.58f;
        e.state=EnemyState::Windup;
      } else if(d>1.18f)moveEnemy(e,hero.pos,e.slow>0?.52f:1.40f,dt);
    } else if(distance(e.pos,e.home)>1.3f) {
      e.state=EnemyState::Return;
      moveEnemy(e,e.home,e.slow>0?.42f:.90f,dt);
    } else {
      e.state=EnemyState::Patrol;
      const float t=time*.32f+e.phase;
      const Vec2 patrol{e.home.x+std::cos(t)*.72f,
                        e.home.y+std::sin(t)*.55f};
      moveEnemy(e,patrol,e.slow>0?.30f:.52f,dt);
    }
  }
}
bool World::save(const std::string& path) const {
  if(path.empty()) return false;
  std::error_code ec;
  const auto parent=std::filesystem::path(path).parent_path();
  if(!parent.empty()) std::filesystem::create_directories(parent,ec);
  if(ec) return false;
  const std::string tmp=path+".tmp";
  std::ofstream f(tmp,std::ios::trunc);
  if(!f) return false;
  f<<"WENUY_LEGENDS_1\n"<<seed_<<' '<<hero.pos.x<<' '<<hero.pos.y<<' '
   <<hero.hp<<' '<<hero.stamina<<' '<<hero.level<<' '<<hero.xp<<' '<<hero.potions<<' '
   <<static_cast<int>(hero.element)<<' '<<defeated<<' '<<collected<<' '<<won<<'\n';
  for(const auto& e:enemies) f<<e.pos.x<<' '<<e.pos.y<<' '<<e.hp<<' '<<e.alive<<'\n';
  for(const auto& c:crystals) f<<c.taken<<' ';
  f<<'\n';
  f<<"AETHERIA_EXTRA_4 "<<sideQuest<<' '<<herbsCollected<<' '<<healerRested<<'\n';
  for(const auto& herb:herbs)f<<herb.taken<<' ';
  f<<'\n';
  f<<"AETHERIA_EXTRA_5 "<<crystalShards<<' '<<essence<<' '<<hero.weapon<<' '<<hero.charm<<'\n';
  f.close();
  if(!f) { std::filesystem::remove(tmp,ec);return false; }
  std::filesystem::rename(tmp,path,ec);
  if(ec){std::error_code ignored;std::filesystem::remove(tmp,ignored);}
  return !ec;
}
bool World::load(const std::string& path) {
  std::ifstream f(path); if(!f) return false;
  std::string tag; std::getline(f,tag); if(tag!="WENUY_LEGENDS_1") return false;
  uint32_t seed=0; float x,y,hp,stamina; int level,xp,potions,el,kills,gems,win;
  if(!(f>>seed>>x>>y>>hp>>stamina>>level>>xp>>potions>>el>>kills>>gems>>win)) return false;
  if(seed==0 || !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(hp)||!std::isfinite(stamina)||
     x<1||y<1||x>=W-1||y>=H-1||hp<0||hp>100||stamina<0||stamina>100||
     level<1||level>100||xp<0||xp>100000||potions<0||potions>99||el<0||el>3||
     kills<0||kills>100||gems<0||gems>100||win<0||win>1) return false;
  World candidate(seed);
  candidate.hero.pos={x,y}; candidate.hero.hp=hp; candidate.hero.stamina=stamina;
  candidate.hero.level=level; candidate.hero.xp=xp; candidate.hero.potions=potions;
  candidate.hero.element=static_cast<Element>(el);
  candidate.won=win!=0; candidate.lost=hp==0;
  for(auto& e:candidate.enemies) {
    int alive;
    if(!(f>>e.pos.x>>e.pos.y>>e.hp>>alive) || !std::isfinite(e.pos.x)||!std::isfinite(e.pos.y)||
       !std::isfinite(e.hp)||e.pos.x<1||e.pos.x>=W-1||e.pos.y<1||e.pos.y>=H-1||
       e.hp<0||e.hp>1000||(alive!=0&&alive!=1)) return false;
    e.alive=alive!=0;
  }
  for(auto& c:candidate.crystals) {
    int taken; if(!(f>>taken)||(taken!=0&&taken!=1)) return false; c.taken=taken!=0;
  }
  // 0.1–0.3 saves end here. 0.4 and 0.5 add validated extensions.
  bool hasStage5=false;
  std::string extension;
  if(f>>extension) {
    if(extension!="AETHERIA_EXTRA_4")return false;
    int quest,herbCount,rested;
    if(!(f>>quest>>herbCount>>rested)||quest<0||quest>3||
       herbCount<0||herbCount>static_cast<int>(candidate.herbs.size())||
       (rested!=0&&rested!=1)) return false;
    for(auto& herb:candidate.herbs) {
      int taken;if(!(f>>taken)||(taken!=0&&taken!=1))return false;
      herb.taken=taken!=0;
    }
    if(herbCount!=static_cast<int>(std::count_if(candidate.herbs.begin(),candidate.herbs.end(),
                               [](const Herb& h){return h.taken;})))return false;
    if((quest==2||quest==3)&&herbCount<3)return false;
    candidate.sideQuest=quest;candidate.herbsCollected=herbCount;
    candidate.healerRested=(rested!=0);
    if(f>>extension){
      if(extension!="AETHERIA_EXTRA_5")return false;
      hasStage5=true;
      int shards,spirit,weapon,charm;
      if(!(f>>shards>>spirit>>weapon>>charm)||shards<0||shards>99||spirit<0||spirit>99||
         weapon<0||weapon>1||charm<0||charm>1)return false;
      candidate.crystalShards=shards;candidate.essence=spirit;
      candidate.hero.weapon=weapon;candidate.hero.charm=charm;
      if(f>>extension)return false; // reject unknown trailing fields
    }else if(!f.eof())return false;
  } else if(!f.eof())return false;
  const int actualGems=static_cast<int>(std::count_if(candidate.crystals.begin(),candidate.crystals.end(),
                                              [](const Crystal& c){return c.taken;}));
  const int actualKills=static_cast<int>(std::count_if(candidate.enemies.begin(),candidate.enemies.end(),
                                              [](const Enemy& e){return !e.alive;}));
  if(actualGems!=gems||actualKills!=kills) return false;
  // Pre-0.5 saves did not store material balances. Give credit for
  // crystals and defeated enemies instead of silently losing prior progress.
  if(!hasStage5){
    candidate.crystalShards=std::min(99,gems);
    candidate.essence=std::min(99,kills);
  }
  // A 0.2 save may place the player inside a tree introduced by 0.3.
  // Recover nearby ground without altering the older file on disk.
  if(candidate.blocked(x,y)) {
    bool relocated=false;
    for(float radius=.4f;radius<=2.01f&&!relocated;radius+=.4f)
      for(int k=0;k<16&&!relocated;++k) {
        const float a=2.f*kPi*k/16.f;
        const Vec2 pos{x+std::cos(a)*radius,y+std::sin(a)*radius};
        if(!candidate.blocked(pos.x,pos.y)) {candidate.hero.pos=pos;relocated=true;}
      }
    if(!relocated)return false;
  }
  candidate.collected=gems;candidate.defeated=kills;
  *this=std::move(candidate);
  say("GAME LOADED");
  return true;
}
} // namespace legends
