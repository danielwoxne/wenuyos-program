#include "Model.h"
#include "Font.h"
#include "MiniSDL.h"
#include "Scene3D.h"
#include "Sound.h"
#include "UiLayout.h"
#include "GameplayAssist.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace legends;
using render::Color;
namespace {
constexpr int SW=960,SH=600,CELL=48;
const Color white{246,249,235},sub{181,204,195};
Color rgba(Color c,uint8_t a) {c.a=a;return c;}
uint32_t mixed(uint32_t x,uint32_t y,uint32_t s) {
  uint32_t z=x*3418731287u ^ y*1328979871u ^ s;
  z^=z>>16;z*=0x7feb352du;z^=z>>15;z*=0x846ca68bu;return z^(z>>16);
}
void ring(SDL_Renderer* r,int cx,int cy,int radius,Color c) {
  render::set(r,c);
  int x=radius,y=0,err=1-radius;
  while(x>=y) {
    SDL_RenderDrawPoint(r,cx+x,cy+y);SDL_RenderDrawPoint(r,cx+y,cy+x);
    SDL_RenderDrawPoint(r,cx-x,cy+y);SDL_RenderDrawPoint(r,cx-y,cy+x);
    SDL_RenderDrawPoint(r,cx-x,cy-y);SDL_RenderDrawPoint(r,cx-y,cy-x);
    SDL_RenderDrawPoint(r,cx+x,cy-y);SDL_RenderDrawPoint(r,cx+y,cy-x);
    ++y;if(err<0)err+=2*y+1;else {--x;err+=2*(y-x+1);}
  }
}
void diamond(SDL_Renderer* r,int x,int y,int rx,int ry,Color c) {
  render::set(r,c);
  for(int dy=-ry;dy<=ry;++dy) {
    int width=rx*(ry-std::abs(dy))/std::max(ry,1);
    SDL_RenderDrawLine(r,x-width,y+dy,x+width,y+dy);
  }
}
void drawText(SDL_Renderer* r,const std::string& s,int x,int y,int size=2,Color color=white,bool shadow=true) {
  render::text(r,s,x,y,size,color,shadow);
}
struct App {
  SDL_Window* window=nullptr;
  SDL_Renderer* renderer=nullptr;
  World world;
  aetheria::Scene3D scene3d;
  aetheria::SceneStats sceneStats{};
  aetheria::Sound sound;
  bool audioEnabled=true,inventory=false,showSideQuest=true;
  bool rightDragging=false;
  aetheria::CameraControl cameraControl;
  aetheria::AutoSaveClock autosaveClock;
  Input input{};
  bool mode3d=true,lowDetail=true,rotateLeft=false,rotateRight=false;
  float cameraYaw=-.785398f;
  bool swedish=false,help=false,bigMap=false,fullscreen=false,quit=false;
  bool screenshotMode=false;
  std::string savePath;
  int frameLimit=0;
  const char* tr(const char* en,const char* sv) const {return swedish?sv:en;}
  explicit App() {
    const char* lang=std::getenv("LANGUAGE");
    if(!lang||!*lang) lang=std::getenv("LANG");
    swedish=lang && lang[0]=='s' && lang[1]=='v';
    const char* d=std::getenv("XDG_DATA_HOME");
    std::string base;
    if(d && *d && d[0]=='/') base=d;
    else {
      const char* h=std::getenv("HOME");
      if(h && *h) base=std::string(h)+"/.local/share";
    }
    if(!base.empty()) savePath=base+"/aetheria/save1.txt";
  }
  ~App() {
    sound.stop();
    if(renderer) SDL_DestroyRenderer(renderer);
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
  }
  bool init() {
    if(SDL_Init(SDL_INIT_VIDEO)!=0) {std::cerr<<SDL_GetError()<<'\n';return false;}
    window=SDL_CreateWindow("Aetheria - Elementens varld",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                            SW,SH,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if(!window) {std::cerr<<SDL_GetError()<<'\n';return false;}
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer) {std::cerr<<SDL_GetError()<<'\n';return false;}
    SDL_RenderSetLogicalSize(renderer,SW,SH);
    SDL_SetRenderDrawBlendMode(renderer,1);
    if(!savePath.empty() && !world.load(savePath)) {
      // Migrate the previous Wenuy Legends save without modifying the original.
      const auto legacy=std::filesystem::path(savePath).parent_path().parent_path()/
                         "wenuy-legends"/"save1.txt";
      if(world.load(legacy.string())) world.save(savePath);
    }
    return true;
  }
  void save() {
    if(savePath.empty()) {world.message="HOME NOT AVAILABLE";world.messageSeconds=2;return;}
    bool ok=world.save(savePath);
    if(ok)autosaveClock.reset();
    world.message=ok?"GAME SAVED":"SAVE FAILED";world.messageSeconds=2;
  }
  void load() {
    bool ok=!savePath.empty() && world.load(savePath);
    if(ok) {
      autosaveClock.reset();input={};rightDragging=false;
    }else {world.message="NO VALID SAVE";world.messageSeconds=2;}
  }
  void mouseClick(int x,int y) {
    const ui::State state{inventory,help,bigMap,world.activeNpc>=0,world.paused,
                          world.won||world.lost,world.nearestNpc()>=0};
    const ui::Action action=ui::hitTest(x,y,state);
    switch(action) {
      case ui::Action::Inventory:
        inventory=true;input={};world.paused=true;return;
      case ui::Action::Map:
        bigMap=true;input={};world.paused=true;return;
      case ui::Action::Help:
        help=true;input={};world.paused=true;return;
      case ui::Action::QuestToggle:
        showSideQuest=!showSideQuest;return;
      case ui::Action::Element:
        world.cycleElement();return;
      case ui::Action::Attack:
        if(world.attack())sound.play(aetheria::Sound::Cue::Strike);return;
      case ui::Action::Skill:
        if(world.skill())sound.play(aetheria::Sound::Cue::Magic);return;
      case ui::Action::Potion:
        if(world.potion())sound.play(aetheria::Sound::Cue::Healing);return;
      case ui::Action::Interact:
        if(world.interact())sound.play(aetheria::Sound::Cue::Dialogue);return;
      case ui::Action::CraftSword:
      case ui::Action::CraftCharm:
        sound.play(world.craft(action==ui::Action::CraftSword?1:2)
                   ?aetheria::Sound::Cue::Craft:aetheria::Sound::Cue::Error);return;
      case ui::Action::DialogueFirst:
      case ui::Action::DialogueSecond:
        if(world.chooseDialogue(action==ui::Action::DialogueSecond?1:0))
          sound.play(aetheria::Sound::Cue::Dialogue);
        return;
      case ui::Action::Resume:
        world.paused=false;return;
      case ui::Action::Close:
        if(inventory){inventory=false;world.paused=help||bigMap;}
        else if(world.activeNpc>=0)world.closeDialogue();
        else if(help){help=false;world.paused=bigMap||inventory;}
        else if(bigMap){bigMap=false;world.paused=help||inventory;}
        return;
      case ui::Action::None:break;
    }
    // UI backgrounds and every modal swallow clicks; never attack through a menu.
    if(state.inventory||state.help||state.map||state.dialogue||state.paused||
       state.ended||ui::status.contains(x,y)||ui::quest.contains(x,y)||
       (x>=340&&x<795&&y<135))return;
    if(world.attack())sound.play(aetheria::Sound::Cue::Strike);
  }
  void event(const SDL_Event& e) {
    if(e.type==SDL_QUIT) {quit=true;return;}
    if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_RIGHT) {
      rightDragging=mode3d&&!inventory&&!help&&!bigMap&&!world.paused&&
                    world.activeNpc<0&&!world.won&&!world.lost;
      return;
    }
    if(e.type==SDL_MOUSEBUTTONUP && e.button.button==SDL_BUTTON_RIGHT) {
      rightDragging=false;return;
    }
    if(e.type==SDL_MOUSEMOTION) {
      if(rightDragging&&mode3d&&!inventory&&!help&&!bigMap&&
         !world.paused&&world.activeNpc<0&&!world.won&&!world.lost)
        cameraYaw=aetheria::CameraControl::dragYaw(cameraYaw,static_cast<float>(e.motion.xrel));
      return;
    }
    if(e.type==SDL_MOUSEWHEEL) {
      if(mode3d&&!inventory&&!help&&!bigMap&&!world.paused&&
         world.activeNpc<0&&!world.won&&!world.lost) cameraControl.wheel(e.wheel.y);
      return;
    }
    if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==1) {
      mouseClick(e.button.x,e.button.y);return;
    }
    if(e.type!=SDL_KEYDOWN && e.type!=SDL_KEYUP) return;
    bool down=e.type==SDL_KEYDOWN;
    int k=e.key.keysym.sym;
    if(down && !e.key.repeat && k=='m') {sound.toggleMute();return;}
    if(inventory) {
      if(!down||e.key.repeat)return;
      if(k=='i'||k==SDLK_ESCAPE){inventory=false;world.paused=help||bigMap;}
      else if(k=='1'||k=='2')sound.play(world.craft(k-'0')?aetheria::Sound::Cue::Craft:aetheria::Sound::Cue::Error);
      return;
    }
    if(world.activeNpc>=0) {
      if(down && !e.key.repeat && (k=='e'||k=='1'||k=='2')){
        if(world.chooseDialogue(k=='2'?1:0))sound.play(aetheria::Sound::Cue::Dialogue);
      }
      else if(down && !e.key.repeat && k==SDLK_ESCAPE)world.closeDialogue();
      if(!down) {
        if(k=='w'||k==SDLK_UP)input.up=false;
        if(k=='s'||k==SDLK_DOWN)input.down=false;
        if(k=='a'||k==SDLK_LEFT)input.left=false;
        if(k=='d'||k==SDLK_RIGHT)input.right=false;
      }
      return;
    }
    if(k=='w'||k==SDLK_UP) input.up=down;
    if(k=='s'||k==SDLK_DOWN) input.down=down;
    if(k=='a'||k==SDLK_LEFT) input.left=down;
    if(k=='d'||k==SDLK_RIGHT) input.right=down;
    if(k==SDLK_LSHIFT||k==SDLK_RSHIFT) input.sprint=down;
    if(k=='j'||k==SDLK_SPACE) input.attack=down;
    if(k=='f') input.jump=down;
    if(k=='g') input.glide=down;
    if(k=='z') rotateLeft=down;
    if(k=='c') rotateRight=down;
    if(!down||e.key.repeat) return;
    switch(k) {
      case 'q': world.cycleElement();break;
      case 'v': mode3d=!mode3d;break;
      case SDLK_F2: lowDetail=!lowDetail;break;
      case 'k': if(world.skill())sound.play(aetheria::Sound::Cue::Magic);break;
      case 'e': if(world.interact())sound.play(aetheria::Sound::Cue::Dialogue);break;
      case 'r': if(world.potion())sound.play(aetheria::Sound::Cue::Healing);break;
      case 'i': inventory=!inventory;input={};world.paused=help||bigMap||inventory;break;
      case 't': showSideQuest=!showSideQuest;break;
      case 'h': help=!help;input={};world.paused=help||bigMap||inventory;break;
      case SDLK_TAB: bigMap=!bigMap;input={};world.paused=help||bigMap||inventory;break;
      case SDLK_ESCAPE: if(help||bigMap||inventory) {help=bigMap=inventory=false;world.paused=false;}
                         else {world.paused=!world.paused;}
                         break;
      case SDLK_F11: fullscreen=!fullscreen;
                        SDL_SetWindowFullscreen(window,fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);break;
      case SDLK_F5: save();break;
      case SDLK_F9: load();break;
      case 'n': if(world.lost||world.won) {world.reset();autosaveClock.reset();}break;
      default: break;
    }
  }
  int sx(float x) const {return SW/2+static_cast<int>((x-world.hero.pos.x)*CELL);}
  int sy(float y) const {return SH/2+static_cast<int>((y-world.hero.pos.y)*CELL);}
  Color tileColor(Tile t,uint32_t v) const {
    int shift=static_cast<int>(v%13)-6;
    Color b{};
    switch(t) {
      case Tile::Grass:b={73,132,99};break;
      case Tile::Forest:b={51,111,88};break;
      case Tile::Sand:b={188,166,111};break;
      case Tile::Stone:b={111,127,125};break;
      case Tile::Water:b={38,88,136};break;
      case Tile::Path:b={152,143,113};break;
    }
    b.r=static_cast<uint8_t>(static_cast<int>(b.r)+shift);
    b.g=static_cast<uint8_t>(static_cast<int>(b.g)+shift);
    b.b=static_cast<uint8_t>(static_cast<int>(b.b)+shift);
    return b;
  }
  void terrain() {
    SDL_Renderer* r=renderer;
    const int left=std::max(0,static_cast<int>(std::floor(world.hero.pos.x-SW/(2*CELL)))-2);
    const int top=std::max(0,static_cast<int>(std::floor(world.hero.pos.y-SH/(2*CELL)))-2);
    const int right=std::min(World::W-1,left+SW/CELL+6);
    const int bottom=std::min(World::H-1,top+SH/CELL+6);
    for(int y=top;y<=bottom;++y) for(int x=left;x<=right;++x) {
      int px=sx(static_cast<float>(x)), py=sy(static_cast<float>(y));
      uint32_t v=mixed(static_cast<uint32_t>(x),static_cast<uint32_t>(y),world.seed());
      Tile t=world.tile(x,y);
      render::rect(r,px,py,CELL+1,CELL+1,tileColor(t,v));
      if(t==Tile::Water) {
        int dy=static_cast<int>((world.time*13+v%32))%CELL;
        render::line(r,px+8,py+dy,px+24,py+dy,{82,156,183,150});
        render::line(r,px+19,py+(dy+22)%CELL,px+32,py+(dy+22)%CELL,{70,136,173,160});
      } else if(t==Tile::Grass||t==Tile::Forest) {
        for(int i=0;i<3;++i) {
          int xx=px+static_cast<int>((v>>(i*7))%40)+4;
          int yy=py+static_cast<int>((v>>(i*5+3))%40)+4;
          render::line(r,xx,yy+4,xx+1,yy-1,{36,99,74});
          if((v>>i)%13==0) {
            render::circle(r,xx+6,yy,2,{235,203,146});
            render::circle(r,xx+6,yy,1,{246,245,208});
          }
        }
      } else if(t==Tile::Stone) {
        render::line(r,px+8,py+13,px+18,py+10,{92,107,112});
      } else if(t==Tile::Path) {
        render::circle(r,px+static_cast<int>(v%36)+6,py+static_cast<int>((v>>5)%35)+5,2,{179,164,132});
      }
    }
    // Scenery in a separate pass, making foliage rise above the ground.
    for(int y=top;y<=bottom;++y) for(int x=left;x<=right;++x) {
      auto t=world.tile(x,y);
      if(t!=Tile::Forest && t!=Tile::Grass) continue;
      uint32_t v=mixed(static_cast<uint32_t>(x),static_cast<uint32_t>(y),world.seed());
      if(v%11!=0||(std::abs(x-42)<4&&std::abs(y-40)<4)) continue;
      int px=sx(static_cast<float>(x)) + CELL/2;
      int py=sy(static_cast<float>(y)) + CELL/2;
      // Foliage shadow, trunk, and four layers of green canopy.
      render::circle(r,px+2,py+11,12,{30,54,47,100});
      render::rect(r,px-4,py-21,8,37,{88,79,61});
      render::circle(r,px-10,py-25,16,{29,91,66});
      render::circle(r,px+9,py-28,17,{38,111,73});
      render::circle(r,px-1,py-37,18,{54,132,82});
      render::circle(r,px-6,py-45,7,{71,152,98});
    }
  }
  void shrine() {
    SDL_Renderer* r=renderer;
    const int x=sx(world.shrine.x),y=sy(world.shrine.y);
    const bool ready=world.questStage()>=2;
    Color glow=ready?Color{134,235,221}:Color{173,169,202};
    render::circle(r,x,y+11,37,{29,45,63,110});
    ring(r,x,y+7,31,glow);
    ring(r,x,y+7,22,rgba(glow,150));
    for(int i=0;i<8;++i) {
      float a=static_cast<float>(i)*.785398f+world.time*.2f;
      int xx=x+static_cast<int>(std::cos(a)*30),yy=y+7+static_cast<int>(std::sin(a)*30);
      diamond(r,xx,yy,3,5,{220,207,163});
    }
    render::rect(r,x-11,y-33,22,40,{91,105,119});
    render::rect(r,x-8,y-38,16,5,{177,177,159});
    diamond(r,x,y-28,10,16,glow);
    diamond(r,x-2,y-33,4,7,{241,250,224});
    if(ready) for(int j=0;j<4;++j) {
      int fx=x+static_cast<int>(std::sin(world.time*2+j*1.57f)*17);
      int fy=y-30-j*12+static_cast<int>(std::sin(world.time*2+j)*5);
      render::circle(r,fx,fy,2,glow);
    }
  }
  void objects() {
    SDL_Renderer* r=renderer;
    shrine();
    for(const auto& c:world.crystals) {
      if(c.taken) continue;
      int x=sx(c.pos.x),y=sy(c.pos.y)+static_cast<int>(std::sin(world.time*3+c.pos.x)*5);
      if(x<-30||x>SW+30||y<-30||y>SH+30) continue;
      render::circle(r,x,y+17,10,{26,65,80,120});
      diamond(r,x,y-8,13,20,{49,189,205});
      diamond(r,x-3,y-12,5,11,{185,249,229});
      ring(r,x,y-7,20,{189,238,217,110});
    }
    for(const auto& herb:world.herbs) if(!herb.taken) {
      int x=sx(herb.pos.x),y=sy(herb.pos.y);
      render::line(r,x,y+8,x,y-12,{94,200,117});
      diamond(r,x-7,y-8,9,6,{136,221,126});
      diamond(r,x+6,y-14,9,6,{165,232,143});
      render::circle(r,x,y-18,4,{241,216,133});
    }
    for(const auto& n:world.npcs) {
      int x=sx(n.pos.x),y=sy(n.pos.y);
      render::circle(r,x,y+13,12,{22,51,55,120});
      render::rect(r,x-8,y-10,16,25,n.kind==0?Color{204,161,104}:Color{165,125,192});
      render::circle(r,x,y-17,10,{231,189,151});
      render::circle(r,x,y-25,11,n.kind==0?Color{184,185,191}:Color{95,75,117});
      diamond(r,x,y-43,5,8,{245,215,135});
    }
    for(const auto& e:world.enemies) {
      if(!e.alive) continue;
      int x=sx(e.pos.x), y=sy(e.pos.y);
      if(x<-30||x>SW+30||y<-30||y>SH+30) continue;
      if(e.windup>0) {
        ring(r,x,y,static_cast<int>(31+e.windup*20),{247,88,77});
        diamond(r,x,y-43,8,9,{249,125,76});
      }
      bool slow=e.slow>0;
      render::circle(r,x,y+13,16,{25,37,55,110});
      render::circle(r,x,y-5,16,slow?Color{123,197,221}:Color{202,113,116});
      render::circle(r,x-7,y-13,7,slow?Color{163,229,238}:Color{235,161,145});
      render::circle(r,x+6,y-15,6,slow?Color{156,215,221}:Color{229,151,151});
      render::circle(r,x-6,y-3,3,{26,37,53});
      render::circle(r,x+6,y-3,3,{26,37,53});
      render::line(r,x-3,y+3,x+3,y+3,{66,49,65});
      if(e.hp<65) {
        render::rect(r,x-18,y-33,36,4,{40,47,51});
        render::rect(r,x-18,y-33,static_cast<int>(36*e.hp/90),4,{232,124,125});
      }
    }
    // Player: an original cloak-wearing explorer, not a licensed character.
    const int hx=SW/2, hy=SH/2;
    render::circle(r,hx,hy+15,15,{26,49,47,120});
    render::rect(r,hx-8,hy+6,7,15,{38,58,91});
    render::rect(r,hx+3,hy+6,7,15,{37,57,90});
    diamond(r,hx,hy,17,23,{43,98,133});
    diamond(r,hx,hy-3,10,18,{63,142,172});
    render::rect(r,hx-14,hy-7,8,17,{219,170,128});
    render::rect(r,hx+7,hy-7,8,17,{218,163,121});
    render::circle(r,hx,hy-24,13,{238,189,146});
    render::circle(r,hx-4,hy-30,13,{55,49,77});
    render::circle(r,hx+5,hy-34,8,{62,55,81});
    render::line(r,hx-4,hy-24,hx-1,hy-24,{39,43,62});
    render::line(r,hx+4,hy-24,hx+7,hy-24,{39,43,62});
    render::rect(r,hx-5,hy-7,10,4,{224,182,122});
    if(world.hero.weapon) {
      render::line(r,hx+14,hy+1,hx+30,hy-30,{167,241,244});
      render::line(r,hx+15,hy+1,hx+29,hy-29,{231,255,250});
      render::line(r,hx+11,hy-6,hx+21,hy-1,{84,182,205});
    }
    if(world.hero.charm) {
      diamond(r,hx,hy-6,5,7,{155,251,186});
      ring(r,hx,hy-6,9,{97,201,162});
    }
    if(world.strikePulse>0) {
      int rad=static_cast<int>(75*(world.strikePulse/.26f));
      ring(r,hx,hy,rad,{249,225,172});
      render::line(r,hx-rad,hy-30,hx+rad,hy+16,{249,241,217,190});
    }
    if(world.skillPulse>0) {
      Color c{};
      switch(world.hero.element) {
        case Element::Fire:c={242,131,78};break;
        case Element::Frost:c={151,225,245};break;
        case Element::Wind:c={132,220,152};break;
        case Element::Thunder:c={232,190,250};break;
      }
      int radius=static_cast<int>((1-world.skillPulse/.55f)*world.skillRadius*CELL);
      for(int d=0;d<3;++d) ring(r,sx(world.pulseAt.x),sy(world.pulseAt.y),radius-d*3,rgba(c,220));
      for(int k=0;k<12;++k) {
        float a=static_cast<float>(k)*.523599f+world.time;
        int xx=sx(world.pulseAt.x)+static_cast<int>(std::cos(a)*radius);
        int yy=sy(world.pulseAt.y)+static_cast<int>(std::sin(a)*radius);
        render::circle(r,xx,yy,4,c);
      }
    }
  }
  std::string elementName() const {
    switch(world.hero.element) {
      case Element::Fire:return tr("FIRE","ELD");
      case Element::Frost:return tr("FROST","IS");
      case Element::Wind:return tr("WIND","VIND");
      case Element::Thunder:return tr("THUNDER","ÅSKA");
    }
    return "?";
  }
  Color elementColor() const {
    switch(world.hero.element) {
      case Element::Fire:return {240,142,96};
      case Element::Frost:return {136,222,243};
      case Element::Wind:return {143,221,163};
      case Element::Thunder:return {213,175,242};
    }
    return white;
  }
  std::string localMessage() const {
    const auto& s=world.message;
    if(!swedish) return s;
    if(s=="FIND THE CRYSTALS") return "HITTA KRISTALLERNA";
    if(s=="CRYSTAL FOUND!") return "KRISTALL HITTAD!";
    if(s=="RESTORED HEALTH") return "HÄLSAN ÅTERSTÄLLD";
    if(s=="THE SHRINE NEEDS MORE POWER") return "ALTARET BEHÖVER MER KRAFT";
    if(s=="THE VALLEY IS FREE!") return "DALEN ÄR BEFRIAD!";
    if(s=="THE JOURNEY ENDS HERE") return "ÄVENTYRET ÄR SLUT";
    if(s=="LEVEL UP!") return "NY NIVÅ!";
    if(s=="GAME SAVED") return "SPELET SPARAT";
    if(s=="GAME LOADED") return "SPELET INLÄST";
    if(s=="AUTO SAVED") return "AUTOMATISKT SPARAT";
    if(s=="AUTO SAVE FAILED") return "AUTOSPARANDE MISSLYCKADES";
    if(s=="SAVE FAILED") return "KUNDE INTE SPARA";
    if(s=="NO VALID SAVE") return "INGEN GILTIG SPARFIL";
    if(s=="HOME NOT AVAILABLE") return "HEMMAPP SAKNAS";
    if(s=="HEALING HERB FOUND")return "LÄKEÖRT HITTAD";
    if(s=="HERB QUEST STARTED")return "ÖRTUPPDRAG STARTAT";
    if(s=="RETURN TO ARIN")return "ÅTERVÄND TILL ARIN";
    if(s=="SIDE QUEST COMPLETE")return "SIDOUPPDRAG KLART";
    if(s=="HEALER HELPED")return "LÄKAREN HJÄLPTE DIG";
    if(s=="RUNE SWORD CRAFTED")return "RUNSVÄRD TILLVERKAT";
    if(s=="FOREST CHARM CRAFTED")return "SKOGSAMULETT TILLVERKAD";
    if(s=="NOT ENOUGH MATERIALS")return "SAKNAR MATERIAL";
    return s;
  }
  void bar(int x,int y,int w,int h,float ratio,Color c) {
    render::rect(renderer,x,y,w,h,{25,43,54,220});
    render::rect(renderer,x+2,y+2,static_cast<int>((w-4)*std::clamp(ratio,0.f,1.f)),h-4,c);
  }
  void minimap(int x,int y,int size,bool large=false) {
    const int tiles=large?47:23;
    int cell=std::max(2,size/tiles);
    render::rect(renderer,x-9,y-9,size+18,size+18,{14,36,51,225});
    const int cx=static_cast<int>(world.hero.pos.x),cy=static_cast<int>(world.hero.pos.y);
    for(int ry=0;ry<tiles;++ry) for(int rx=0;rx<tiles;++rx) {
      int wx=cx+rx-tiles/2,wy=cy+ry-tiles/2;
      auto t=world.tile(wx,wy);
      Color c=tileColor(t,mixed(static_cast<uint32_t>(wx),static_cast<uint32_t>(wy),world.seed()));
      render::rect(renderer,x+rx*cell,y+ry*cell,cell,cell,c);
    }
    auto point=[&](Vec2 p,Color c,int radius) {
      int mx=x+(static_cast<int>(p.x)-cx+tiles/2)*cell+cell/2;
      int my=y+(static_cast<int>(p.y)-cy+tiles/2)*cell+cell/2;
      if(mx>=x&&mx<x+tiles*cell&&my>=y&&my<y+tiles*cell) render::circle(renderer,mx,my,radius,c);
    };
    point(world.shrine,{227,219,206},large?5:3);
    for(const auto& c:world.crystals) if(!c.taken) point(c.pos,{176,249,240},large?4:2);
    for(const auto& e:world.enemies) if(e.alive) point(e.pos,{231,123,133},large?3:2);
    for(const auto& herb:world.herbs) if(!herb.taken) point(herb.pos,{141,241,139},large?4:2);
    for(const auto& n:world.npcs) point(n.pos,{252,213,135},large?5:3);
    const auto target=aetheria::nearestQuestTarget(world);
    if(target.valid()) {
      const int mx=x+(static_cast<int>(target.point.x)-cx+tiles/2)*cell+cell/2;
      const int my=y+(static_cast<int>(target.point.y)-cy+tiles/2)*cell+cell/2;
      if(mx>=x&&mx<x+tiles*cell&&my>=y&&my<y+tiles*cell) {
        render::circle(renderer,mx,my,large?6:4,{255,224,125});
        ring(renderer,mx,my,large?10:7,{255,245,181});
      }
    }
    point(world.hero.pos,white,large?6:4);
    render::rect(renderer,x-9,y-9,size+18,3,{200,199,174});
  }
  const char* targetName(aetheria::GoalKind kind) const {
    switch(kind) {
      case aetheria::GoalKind::Crystal:return tr("CRYSTAL","KRISTALL");
      case aetheria::GoalKind::Guard:return tr("GUARD","VAKT");
      case aetheria::GoalKind::Shrine:return tr("SHRINE","ALTARE");
      case aetheria::GoalKind::Elder:return tr("ELDER ARIN","ÄLDSTE ARIN");
      case aetheria::GoalKind::Herb:return tr("HEALING HERB","LÄKEÖRT");
      default:return tr("EXPLORE","UTFORSKA");
    }
  }
  void uiButton(const ui::Rect& r,Color fill,Color edge) {
    render::rect(renderer,r.x,r.y,r.w,r.h,edge);
    render::rect(renderer,r.x+2,r.y+2,r.w-4,r.h-4,fill);
  }
  void wrappedText(const std::string& s,int x,int y,int maxWidth,int maxLines,
                   int scale,Color color) {
    std::istringstream words(s);std::string word,line;
    int drawn=0;
    while(words>>word) {
      std::string next=line.empty()?word:line+" "+word;
      if(render::textWidth(next,scale)<=maxWidth) {line=std::move(next);continue;}
      if(!line.empty()) {
        drawText(renderer,line,x,y+drawn*21,scale,color);
        if(++drawn>=maxLines)return;
      }
      line=std::move(word);
    }
    if(!line.empty()&&drawn<maxLines)
      drawText(renderer,line,x,y+drawn*21,scale,color);
  }
  void hud() {
    SDL_Renderer* r=renderer;
    render::rect(r,ui::status.x,ui::status.y,ui::status.w,ui::status.h,{14,35,49,230});
    render::rect(r,ui::status.x,ui::status.y,4,ui::status.h,{80,185,198});
    drawText(r,"AETHERIA",31,24,3,{239,233,207});
    drawText(r,std::string(tr("LV","NIV"))+" "+std::to_string(world.hero.level),218,30,2,sub);
    drawText(r,"HP",34,67,1,white);
    bar(61,64,176,16,world.hero.hp/100.f,{224,111,116});
    drawText(r,std::to_string(static_cast<int>(world.hero.hp))+"/100",244,67,1,white);
    drawText(r,tr("SP","ORK"),34,94,1,white);
    bar(61,90,176,13,world.hero.stamina/100.f,{105,203,163});
    drawText(r,std::to_string(static_cast<int>(world.hero.stamina))+"/100",244,94,1,white);

    render::rect(r,ui::quest.x,ui::quest.y,ui::quest.w,ui::quest.h,{14,35,49,222});
    render::rect(r,ui::quest.x,ui::quest.y,3,ui::quest.h,{87,183,186});
    drawText(r,tr("MAIN QUEST","HUVUDUPPDRAG"),31,146,1,{164,232,216});
    std::string goal,progress;
    switch(world.questStage()) {
      case 0:goal=tr("FIND FIVE CRYSTALS","HITTA FEM KRISTALLER");
             progress=std::to_string(world.collected)+" / 5";break;
      case 1:goal=tr("DEFEAT FOUR GUARDS","BESEGRA FYRA VAKTER");
             progress=std::to_string(world.defeated)+" / 4";break;
      case 2:goal=tr("ACTIVATE THE SHRINE","AKTIVERA ALTARET");
             progress=tr("USE E NEAR THE SHRINE","TRYCK E VID ALTARET");break;
      default:goal=tr("MISSION COMPLETED","UPPDRAG KLART");
              progress=tr("THE VALLEY IS SAFE","DALEN ÄR BEFRIAD");break;
    }
    drawText(r,goal,31,165,2,white);
    drawText(r,progress,32,190,1,{168,235,220});

    if(showSideQuest) {
      render::rect(r,ui::sideQuest.x,ui::sideQuest.y,ui::sideQuest.w,ui::sideQuest.h,{14,35,49,219});
      render::rect(r,ui::sideQuest.x,ui::sideQuest.y,3,ui::sideQuest.h,{218,189,126});
      drawText(r,tr("SIDE QUEST","SIDOUPPDRAG"),32,217,1,{245,219,162});
      drawText(r,tr("[T] HIDE","[T] DÖLJ"),259,217,1,sub);
      drawText(r,tr("HEALING HERBS","LÄKEÖRTER"),32,231,2,white);
      std::string secondary;
      if(world.sideQuest==0)secondary=tr("TALK TO ELDER ARIN","PRATA MED ÄLDSTE ARIN");
      else if(world.sideQuest==1)secondary=std::to_string(world.herbsCollected)+" / 3";
      else if(world.sideQuest==2)secondary=tr("RETURN TO ARIN","ÅTERVÄND TILL ARIN");
      else secondary=tr("COMPLETE +2 POTIONS","KLART +2 DRYCKER");
      drawText(r,secondary,32,257,1,{188,221,193});
    }else {
      render::rect(r,16,211,322,37,{14,35,49,217});
      drawText(r,tr("[T] SHOW SIDE QUEST","[T] VISA SIDOUPPDRAG"),31,222,2,sub);
    }

    uiButton(ui::inventoryTab,{27,56,68,232},{81,149,158});
    uiButton(ui::mapTab,{27,56,68,232},{81,149,158});
    uiButton(ui::helpTab,{27,56,68,232},{81,149,158});
    drawText(r,tr("[I] BAG","[I] VÄSKA"),365,28,2,white);
    drawText(r,tr("[TAB] MAP","[TAB] KARTA"),500,28,2,white);
    drawText(r,tr("[H] GUIDE","[H] HJÄLP"),649,28,2,white);

    drawText(r,mode3d?tr("3D VIEW","3D VY"):tr("2D VIEW","2D VY"),351,72,2,white);
    drawText(r,mode3d?(lowDetail?tr("F2: SIMPLE GRAPHICS","F2: ENKEL GRAFIK"):
                                 tr("F2: HIGH DETAIL","F2: HÖG DETALJ")):
                      tr("V: 3D VIEW","V: 3D VY"),351,96,1,sub);
    drawText(r,tr("Z/C / RIGHT-DRAG: TURN  WHEEL: ZOOM",
                  "Z/C / HÖGERDRAG: VRID  HJUL: ZOOMA"),351,112,1,sub);
    minimap(817,26,118);
    const auto target=aetheria::nearestQuestTarget(world);
    if(target.valid()) {
      render::rect(r,352,148,430,53,{13,36,51,229});
      render::rect(r,352,148,4,53,{243,206,119});
      drawText(r,tr("NEAREST OBJECTIVE","NÄRMASTE MÅL"),366,154,1,{232,211,153});
      drawText(r,targetName(target.kind),366,176,2,white);
      drawText(r,std::to_string(static_cast<int>(std::round(target.distance)))+
                  tr(" M"," M"),620,176,2,{255,228,157});
      const float angle=aetheria::directionToTarget(world.hero.pos,target.point,cameraYaw);
      const auto arrow=aetheria::compassOffset(angle);
      const int cx=755,cy=176;
      const int tipx=cx+static_cast<int>(std::round(arrow.x*16.f));
      const int tipy=cy+static_cast<int>(std::round(arrow.y*16.f));
      const int stemx=cx+static_cast<int>(std::round(arrow.x*6.f));
      const int stemy=cy+static_cast<int>(std::round(arrow.y*6.f));
      render::line(r,cx,cy,tipx,tipy,{255,230,154});
      // Arrowhead uses a perpendicular to the camera-relative direction.
      render::line(r,tipx,tipy,
                   stemx+static_cast<int>(std::round(-arrow.y*5.f)),
                   stemy+static_cast<int>(std::round( arrow.x*5.f)),{255,230,154});
      render::line(r,tipx,tipy,
                   stemx-static_cast<int>(std::round(-arrow.y*5.f)),
                   stemy-static_cast<int>(std::round( arrow.x*5.f)),{255,230,154});
    }
    drawText(r,tr("NORTH","NORR"),847,154,1,sub);
    drawText(r,world.daylight()<.35f?tr("NIGHT","NATT"):
              world.daylight()<.72f?tr("DUSK","SKYMNING"):tr("DAY","DAG"),821,176,1,white);

    render::rect(r,16,516,928,68,{12,33,49,231});
    render::rect(r,16,516,928,3,{98,175,183});
    uiButton(ui::elementalAction,{28,56,70,247},{64,139,153});
    uiButton(ui::attackAction,{28,56,70,247},{64,139,153});
    const int seconds=static_cast<int>(std::ceil(world.hero.skillCooldown));
    uiButton(ui::skillAction,seconds?Color{34,51,58,247}:Color{28,56,70,247},
             seconds?Color{83,105,110}:Color{64,139,153});
    uiButton(ui::potionAction,{28,56,70,247},{64,139,153});
    drawText(r,"[Q] "+elementName(),43,546,2,elementColor());
    drawText(r,tr("[J] ATTACK","[J] ATTACK"),272,546,2,white);
    drawText(r,"[K] "+elementName()+" "+(seconds?std::to_string(seconds)+"S":"OK"),
             499,546,2,seconds?sub:elementColor());
    drawText(r,"[R] "+std::to_string(world.hero.potions)+" "+tr("POTIONS","DRYCKER"),
             730,546,2,white);
    render::rect(r,16,486,262,25,{14,35,49,215});
    drawText(r,tr("WASD MOVE   H HELP","WASD GÅ   H HJÄLP"),25,493,1,white);

    if(world.activeNpc<0 && world.nearestNpc()>=0) {
      uiButton(ui::interactPrompt,{55,57,52,237},{226,190,131});
      drawText(r,tr("[E] TALK TO VILLAGER","[E] PRATA MED BYBO"),352,474,2,{255,228,171});
    }
    if(world.messageSeconds>0&&world.activeNpc<0) {
      const auto m=localMessage();const int width=render::textWidth(m,2);
      render::rect(r,SW/2-width/2-16,351,width+32,37,{17,43,58,230});
      drawText(r,m,SW/2-width/2,362,2,{235,241,213});
    }
  }
  void overlayBase(const std::string& title,int x,int y,int w,int h) {
    render::rect(renderer,0,0,SW,SH,{7,20,33,188});
    render::rect(renderer,x,y,w,h,{23,48,61,246});
    render::rect(renderer,x,y,w,6,{96,193,198});
    drawText(renderer,title,x+28,y+25,3,{241,236,215});
  }
  void overlay() {
    SDL_Renderer* r=renderer;
    if(inventory){
      overlayBase(tr("EXPLORER'S INVENTORY","ÄVENTYRARENS VÄSKA"),111,50,738,497);
      render::rect(r,137,113,324,362,{16,38,54,248});
      render::rect(r,476,113,344,362,{19,45,59,248});
      uiButton(ui::inventoryClose,{38,65,74,250},{101,172,177});
      drawText(r,tr("CLOSE","STÄNG"),743,85,2,white);
      uiButton(ui::inventorySword,world.hero.weapon?Color{29,44,51,247}:Color{42,65,64,247},
               {114,168,151});
      uiButton(ui::inventoryCharm,world.hero.charm?Color{29,44,51,247}:Color{42,65,64,247},
               {114,168,151});
      drawText(r,tr("MATERIALS","MATERIAL"),158,130,3,{161,228,207});
      drawText(r,tr("CRYSTAL SHARDS","KRISTALLSKÄRVOR"),158,188,2,white);
      drawText(r,std::to_string(world.crystalShards),386,188,2,{154,246,232});
      drawText(r,tr("ELEMENTAL ESSENCE","ELEMENTESSENS"),158,221,2,white);
      drawText(r,std::to_string(world.essence),386,221,2,{231,201,248});
      drawText(r,tr("HEALING POTIONS","HÄLSODRYCKER"),158,254,2,white);
      drawText(r,std::to_string(world.hero.potions),386,254,2,{251,201,139});
      drawText(r,tr("EQUIPPED","UTRUSTAT"),158,320,3,{161,228,207});
      drawText(r,world.hero.weapon?tr("RUNE SWORD","RUNSVÄRD"):
              tr("STARTER BLADE","STARTSVÄRD"),158,368,2,white);
      drawText(r,world.hero.charm?tr("FOREST CHARM","SKOGSAMULETT"):
              tr("NO CHARM","INGEN AMULETT"),158,407,2,white);
      drawText(r,tr("CRAFTING","TILLVERKNING"),495,130,3,{251,214,155});
      drawText(r,tr("[1] RUNE SWORD","[1] RUNSVÄRD"),495,188,2,
               world.hero.weapon?sub:Color{245,228,180});
      drawText(r,world.hero.weapon?tr("ALREADY OWNED","REDAN TILLVERKAT"):
               tr("2 SHARDS + 2 ESSENCE","2 SKÄRVOR + 2 ESSENS"),495,216,2,sub);
      drawText(r,tr("+18 SWORD DAMAGE","+18 SVÄRDSKADA"),495,241,2,{172,230,223});
      drawText(r,tr("[2] FOREST CHARM","[2] SKOGSAMULETT"),495,296,2,
               world.hero.charm?sub:Color{245,228,180});
      drawText(r,world.hero.charm?tr("ALREADY OWNED","REDAN TILLVERKAD"):
               tr("1 SHARD + 3 ESSENCE","1 SKÄRVA + 3 ESSENS"),495,324,2,sub);
      drawText(r,tr("FASTER STAMINA REGEN","SNABBARE ÅTERHÄMTNING"),495,348,2,{172,230,223});
      drawText(r,tr("20% DAMAGE REDUCTION","20% MINDRE SKADA"),495,370,2,{172,230,223});
      drawText(r,tr("[I / ESC] CLOSE","[I / ESC] STÄNG"),344,498,2,{170,223,217});
    }else if(world.activeNpc>=0 && !help && !bigMap) {
      render::rect(r,0,0,SW,SH,{7,15,25,118});
      render::rect(r,85,326,790,216,{14,34,48,247});
      render::rect(r,85,326,790,5,{182,169,116});
      uiButton(ui::dialogueClose,{38,65,74,250},{124,156,164});
      drawText(r,"X",811,350,2,white);
      drawText(r,world.dialogueSpeaker(swedish),111,350,3,{246,220,153});
      wrappedText(world.dialogueText(swedish),111,388,703,3,2,white);
      const auto choices=world.dialogueChoices(swedish);
      if(choices.size()>=2) {
        uiButton(ui::dialogueFirst,{29,67,68,249},{100,195,173});
        uiButton(ui::dialogueSecond,{51,56,62,249},{185,174,121});
        wrappedText(choices[0],116,454,325,2,2,{194,246,220});
        wrappedText(choices[1],489,454,325,2,2,{246,220,170});
      }
      drawText(r,tr("[1]/[2] CHOOSE   [ESC] CLOSE","[1]/[2] VÄLJ   [ESC] STÄNG"),111,517,1,sub);
    } else if(help) {
      overlayBase(tr("EXPLORER'S GUIDE","ÄVENTYRARENS GUIDE"),170,70,620,455);
      uiButton(ui::helpClose,{38,65,74,249},{108,171,173});
      drawText(r,tr("CLOSE","STÄNG"),673,98,2,white);
      const std::vector<std::string> labels={
        tr("WASD / ARROWS   WALK","WASD / PILAR    GÅ"),
        tr("SHIFT           SPRINT (STAMINA)","SHIFT           SPRING (ORK)"),
        tr("F / HOLD G      JUMP / GLIDE","F / HÅLL G      HOPPA / GLID"),
        tr("Z/C/RMB + WHEEL CAMERA / ZOOM","Z/C/HÖGER + HJUL KAMERA / ZOOM"),
        tr("F2              DETAIL LEVEL","F2              DETALJNIVÅ"),
        tr("J / SPACE / CLICK   SWORD ATTACK","J / MELLANSLAG / KLICK   SLÅ"),
        tr("Q               CHANGE ELEMENT","Q               BYT ELEMENT"),
        tr("K               ELEMENTAL SKILL","K               ELEMENTKRAFT"),
        tr("E               TALK / USE SHRINE","E               PRATA / ANVÄND ALTARE"),
        tr("R               HEALING POTION","R               HÄLSODRYCK"),
        tr("I / 1 / 2       INVENTORY / CRAFT","I / 1 / 2       VÄSKA / TILLVERKA"),
        tr("T/M             SIDE QUEST / SOUND","T/M             SIDOUPPDRAG / LJUD"),
        tr("TAB             LARGE MAP","TAB             STOR KARTA"),
        tr("F5 / F9         SAVE / LOAD","F5 / F9         SPARA / LÄS IN"),
        tr("ESC / F11       PAUSE / FULLSCREEN","ESC / F11       PAUS / HELSKÄRM")
      };
      for(size_t i=0;i<labels.size();++i) drawText(r,labels[i],205,124+static_cast<int>(i)*24,2,white);
      drawText(r,tr("PRESS H TO RETURN","TRYCK H FÖR ATT ÅTERVÄNDA"),290,497,2,{152,225,211});
    } else if(bigMap) {
      overlayBase(tr("EXPLORATION MAP","UTFORSKNINGSKARTA"),125,32,710,540);
      uiButton(ui::mapClose,{38,65,74,249},{108,171,173});
      drawText(r,tr("CLOSE","STÄNG"),704,59,2,white);
      minimap(324,78,310,true);
      drawText(r,tr("WHITE = YOU  CYAN = CRYSTAL  GREEN = HERB","VIT = DU  TURKOS = KRISTALL  GRÖN = ÖRT"),180,425,2,white);
      drawText(r,tr("RED = ENEMY  GOLD = VILLAGER","RÖD = FIENDE  GULD = BYBO"),180,456,2,white);
      drawText(r,tr("TAB = CLOSE","TAB = STÄNG"),400,510,2,{144,223,208});
    } else if(world.paused) {
      overlayBase(tr("PAUSED","PAUSAT"),278,194,400,190);
      uiButton(ui::pauseResume,{38,65,74,250},{105,178,175});
      drawText(r,tr("[ESC] CONTINUE","[ESC] FORTSÄTT"),363,286,2,white);
      drawText(r,tr("F5 TO SAVE GAME","F5 FÖR ATT SPARA"),330,336,2,sub);
    } else if(world.won||world.lost) {
      overlayBase(world.won?tr("VALLEY RESTORED!","DALEN ÄR BEFRIAD!"):
                tr("THE JOURNEY ENDS","ÄVENTYRET ÄR SLUT"),192,185,570,225);
      drawText(r,world.won?tr("YOU RESTORED THE OLD SHRINE","DU VÄCKTE DET GAMLA ALTARET"):
                         tr("TRY AGAIN, EXPLORER","FÖRSÖK IGEN, ÄVENTYRARE"),244,278,2,white);
      drawText(r,tr("PRESS N FOR A NEW JOURNEY","TRYCK N FÖR ETT NYTT ÄVENTYR"),240,352,2,{157,232,215});
    }
  }
  void draw(bool present=true) {
    SDL_SetRenderDrawColor(renderer,39,75,103,255);SDL_RenderClear(renderer);
    if(mode3d) sceneStats=scene3d.render(renderer,world,cameraYaw,lowDetail,SW,SH,
                                       cameraControl.zoom());
    else {terrain();objects();}
    hud();
    if(help||bigMap||inventory||world.paused||world.won||world.lost||world.activeNpc>=0) overlay();
    if(present) SDL_RenderPresent(renderer);
  }
  bool capture(const std::string& path) {
    std::vector<uint32_t> pixels(SW*SH);
    if(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),SW*4)!=0) {
      std::cerr<<SDL_GetError()<<'\n';return false;
    }
    std::ofstream out(path,std::ios::binary);
    if(!out) return false;
    out<<"P6\n"<<SW<<' '<<SH<<"\n255\n";
    for(uint32_t pix:pixels) {
      char b[3]={static_cast<char>((pix>>16)&0xff),static_cast<char>((pix>>8)&0xff),static_cast<char>(pix&0xff)};
      out.write(b,3);
    }
    return out.good();
  }
  int run(int argc,char** argv) {
    std::string screenshotPath;
    float previewTime=-1.f;bool previewDialogue=false,previewInventory=false,previewGear=false;
    for(int i=1;i<argc;++i) {
      if(std::string(argv[i])=="--preview-time"&&i+1<argc)
        previewTime=std::max(0.f,static_cast<float>(std::atof(argv[++i])));
      if(std::string(argv[i])=="--screenshot"&&i+1<argc) screenshotPath=argv[++i];
      if(std::string(argv[i])=="--frames"&&i+1<argc) frameLimit=std::max(1,std::atoi(argv[++i]));
      if(std::string(argv[i])=="--preview-dialogue")previewDialogue=true;
      if(std::string(argv[i])=="--preview-inventory")previewInventory=true;
      if(std::string(argv[i])=="--preview-gear")previewGear=true;
      if(std::string(argv[i])=="--no-audio")audioEnabled=false;
    }
    for(int i=1;i<argc;++i) {
      if(std::string(argv[i])=="--2d") mode3d=false;
      if(std::string(argv[i])=="--high-detail") lowDetail=false;
    }
    screenshotMode=!screenshotPath.empty();
    if(!init()) return 1;
    if(previewTime>=0.f)world.time=std::min(previewTime,86400.f);
    if(!screenshotMode && audioEnabled)sound.start();
    if(screenshotMode && previewGear){
      // Only modifies the ephemeral screenshot scene, not persistent saves.
      world.crystalShards=4;world.essence=6;
      world.craft(1);world.craft(2);
      world.messageSeconds=0;
    }
    if(screenshotMode && previewInventory){inventory=true;world.paused=true;}
    if(screenshotMode && previewDialogue && !world.npcs.empty()) {
      world.hero.pos={world.npcs[0].pos.x,world.npcs[0].pos.y-1.4f};
      world.interact();
    }
    uint32_t previous=SDL_GetTicks();int frames=0;
    while(!quit) {
      SDL_Event e{};while(SDL_PollEvent(&e)) event(e);
      uint32_t now=SDL_GetTicks();
      float dt=std::clamp((now-previous)/1000.f,0.f,.05f);previous=now;
      if(rotateLeft) cameraYaw-=dt*1.45f;
      if(rotateRight) cameraYaw+=dt*1.45f;
      Input movement=input;
      if(mode3d) {
        // Camera-relative W/A/S/D, preserving the legacy 2D input model.
        const float forward=float(input.up)-float(input.down);
        const float side=float(input.right)-float(input.left);
        movement.analog=true;
        movement.moveX=forward*std::cos(cameraYaw)+side*std::sin(cameraYaw);
        movement.moveY=forward*std::sin(cameraYaw)-side*std::cos(cameraYaw);
      }
      const int gemsBefore=world.collected, herbsBefore=world.herbsCollected;
      const int killsBefore=world.defeated;
      const float hpBefore=world.hero.hp, swingBefore=world.strikePulse;
      world.update(dt,movement);
      const bool active=!world.paused && world.activeNpc<0 && !inventory &&
                        !help && !bigMap && !world.won && !world.lost;
      if(!screenshotMode && !savePath.empty() && autosaveClock.advance(dt,active)) {
        const bool saved=world.save(savePath);
        autosaveClock.complete(saved);
        world.message=saved?"AUTO SAVED":"AUTO SAVE FAILED";
        world.messageSeconds=2.f;
      }
      if(world.collected>gemsBefore||world.herbsCollected>herbsBefore)sound.play(aetheria::Sound::Cue::Crystal);
      else if(world.defeated>killsBefore)sound.play(aetheria::Sound::Cue::Crystal);
      if(world.hero.hp<hpBefore)sound.play(aetheria::Sound::Cue::Damage);
      if(world.strikePulse>swingBefore+.04f)sound.play(aetheria::Sound::Cue::Strike);
      draw(!screenshotMode);
      ++frames;
      if(screenshotMode && frames==2) return capture(screenshotPath)?0:2;
      if(frameLimit>0&&frames>=frameLimit) break;
      // 30 FPS: low CPU by design, vsync if the driver supports it.
      const uint32_t taken=SDL_GetTicks()-now;
      if(taken<33) SDL_Delay(33-taken);
    }
    if(!screenshotMode && !savePath.empty() && !world.lost) world.save(savePath);
    return 0;
  }
};
} // namespace
int main(int argc,char** argv) {
  App app;return app.run(argc,argv);
}
