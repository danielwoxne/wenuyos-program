#include "Scene3D.h"
#include "Font.h"
#include "Environment.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace aetheria {
namespace {
using legends::World;
using legends::Tile;
using render::Color;
constexpr float kPi=3.141592653589793238f;
struct V3 {float x,y,z;};
V3 operator+(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V3 operator-(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 operator*(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 norm(V3 a){float n=std::sqrt(dot(a,a));return n>.0001f?a*(1.f/n):V3{0,0,1};}
struct Triangle { std::array<SDL_Vertex,3> v; float depth=0; };
uint32_t hxy(int x,int y,uint32_t seed){return legends::terrainHashXY(x,y,seed);}
Color mix(Color a,Color b,float t) {
 t=std::clamp(t,0.f,1.f);
 auto channel=[&](uint8_t x,uint8_t y)->uint8_t{return static_cast<uint8_t>(x*(1-t)+y*t+.5f);};
 return {channel(a.r,b.r),channel(a.g,b.g),channel(a.b,b.b),channel(a.a,b.a)};
}
Color shade(Color c,float mul){
 auto f=[&](uint8_t x){return static_cast<uint8_t>(std::clamp(x*mul,0.f,255.f));};
 return {f(c.r),f(c.g),f(c.b),c.a};
}
Color biome(Tile t,uint32_t h){
 Color c;
 switch(t){
  case Tile::Grass:c={103,169,103};break;
  case Tile::Forest:c={54,127,85};break;
  case Tile::Sand:c={202,187,128};break;
  case Tile::Stone:c={129,138,145};break;
  case Tile::Water:c={43,119,160};break;
  case Tile::Path:c={168,153,115};break;
 }
 return shade(c,.95f+(h%23)*.0043f);
}
struct Camera {
 V3 pos,right,up,forward;
 float focal,cx,cy;
 Camera(const World& w,float yaw,int sw,int sh) {
   V3 focus={w.hero.pos.x+std::cos(yaw)*3.9f,
             w.hero.pos.y+std::sin(yaw)*3.9f,w.height(w.hero.pos.x,w.hero.pos.y)+1.25f};
   pos={w.hero.pos.x-std::cos(yaw)*6.7f,
        w.hero.pos.y-std::sin(yaw)*6.7f,w.height(w.hero.pos.x,w.hero.pos.y)+5.25f};
   forward=norm(focus-pos);right=norm(cross(forward,{0,0,1}));up=norm(cross(right,forward));
   focal=static_cast<float>(sh)*1.06f;cx=sw*.5f;cy=sh*.46f;
 }
 bool project(V3 p,SDL_FPoint& o,float& depth) const {
   V3 d=p-pos;depth=dot(d,forward);
   if(depth<.32f) return false;
   const float inv=focal/depth;
   o={cx+dot(d,right)*inv,cy-dot(d,up)*inv};
   return std::isfinite(o.x)&&std::isfinite(o.y) && std::abs(o.x)<32000 &&std::abs(o.y)<32000;
 }
};
class Painter {
 public:
  explicit Painter(const Camera& c,int width,int height,float daylight)
       :cam(c),sw(width),sh(height),day(daylight){}
  void triangle(V3 a,V3 b,V3 c,Color col, float lighting=1.f) {
    SDL_FPoint p[3];float depth[3];
    if(!cam.project(a,p[0],depth[0])||!cam.project(b,p[1],depth[1])||!cam.project(c,p[2],depth[2]))return;
    if((p[0].x<0&&p[1].x<0&&p[2].x<0) ||
       (p[0].x>sw&&p[1].x>sw&&p[2].x>sw) ||
       (p[0].y<0&&p[1].y<0&&p[2].y<0) ||
       (p[0].y>sh&&p[1].y>sh&&p[2].y>sh))return;
    float minX=std::min({p[0].x,p[1].x,p[2].x});
    float maxX=std::max({p[0].x,p[1].x,p[2].x});
    float minY=std::min({p[0].y,p[1].y,p[2].y});
    float maxY=std::max({p[0].y,p[1].y,p[2].y});
    if((maxX-minX)*(maxY-minY)<.8f)return;
    Triangle t;t.depth=(depth[0]+depth[1]+depth[2])/3;
    const V3 n=norm(cross(b-a,c-a));
    const V3 sun=norm({.65f,-.40f,.75f});
    const float diffuse=.66f+.34f*std::abs(dot(n,sun));
    const float illumination=(.28f+.75f*day)*diffuse*lighting;
    const float fog=std::clamp((t.depth-13.f)/40.f,0.f,.80f);
    const Color mist=mix({33,50,93,255},{155,184,197,255},day);
    Color color=mix(shade(col,illumination),mist,fog);
    for(int i=0;i<3;++i) t.v[i]={p[i],{color.r,color.g,color.b,color.a},{0,0}};
    triangles.push_back(t);
  }
  void quad(V3 a,V3 b,V3 c,V3 d,Color color,float light=1.f){
    triangle(a,b,c,color,light);triangle(a,c,d,color,light);
  }
  // Camera-facing sprite polygon, still a pair of textured-free 3D triangles.
  void billboard(float x,float y,float z,float wid,float hei,Color color,float taper=1.f){
    V3 side=cam.right*(wid*.5f);
    V3 base={x,y,z}; V3 top={x,y,z+hei};
    quad(base-side,base+side,top+side*taper,top-side*taper,color);
  }
  // Simple solid geometry used by characters, rocks, ruins and vegetation.
  void prism(V3 a,V3 b,float width,Color c,float light=1.f){
    const V3 axis=norm(b-a);
    const V3 guide=std::abs(axis.z)>.89f?V3{0,1,0}:V3{0,0,1};
    const V3 side=norm(cross(axis,guide))*(width*.5f);
    const V3 front=norm(cross(axis,side))*(width*.42f);
    const V3 v0=a-side-front,v1=a+side-front,v2=a+side+front,v3=a-side+front;
    const V3 w0=b-side-front,w1=b+side-front,w2=b+side+front,w3=b-side+front;
    quad(v0,v1,w1,w0,c,light);quad(v1,v2,w2,w1,c,light);
    quad(v2,v3,w3,w2,c,light);quad(v3,v0,w0,w3,c,light);
    quad(w0,w1,w2,w3,shade(c,1.07f),light);quad(v3,v2,v1,v0,shade(c,.84f),light);
  }
  void crystal(V3 at,float width,float height,Color c,float intensity=1.f){
    const V3 north{0,width*.5f,0},east{width*.5f,0,0};
    const V3 tip=at+V3{0,0,height},base=at+V3{0,0,-height*.35f};
    triangle(at+north,at+east,tip,c,intensity);
    triangle(at+east,at-north,tip,shade(c,.83f),intensity);
    triangle(at-north,at-east,tip,c,intensity);
    triangle(at-east,at+north,tip,shade(c,1.15f),intensity);
    triangle(at+east,at+north,base,c,intensity);
    triangle(at-north,at+east,base,c,intensity);
    triangle(at-east,at-north,base,c,intensity);
    triangle(at+north,at-east,base,c,intensity);
  }
  void cone(float x,float y,float z,float width,float height,Color color){
    V3 side=cam.right*(width*.5f);V3 center={x,y,z};V3 apex={x,y,z+height};
    triangle(center-side,center+side,apex,color);
    triangle(center-side*.7f,center+side*.7f,apex+cam.forward*.2f,shade(color,1.12f));
  }
  int flush(SDL_Renderer* renderer){
    std::sort(triangles.begin(),triangles.end(),[](const Triangle& a,const Triangle& b){return a.depth>b.depth;});
    std::vector<SDL_Vertex> vertices;vertices.reserve(triangles.size()*3);
    for(const auto& t:triangles)for(const auto& v:t.v)vertices.push_back(v);
    if(!vertices.empty()) SDL_RenderGeometry(renderer,nullptr,vertices.data(),static_cast<int>(vertices.size()),nullptr,0);
    return static_cast<int>(triangles.size());
  }
 private:
  const Camera& cam;int sw,sh;float day;std::vector<Triangle> triangles;
};
void sky(SDL_Renderer* r,int sw,int sh,float yaw,float daylight,float time) {
  const Color zenith=mix({11,21,57},{74,140,205},daylight);
  const Color horizon=mix({59,72,113},{176,216,235},daylight);
  for(int j=0;j<22;++j){
    const float t=j/21.f;
    Color c=mix(zenith,horizon,t);
    render::rect(r,0,j*sh/24,sw,sh/24+2,c);
  }
  render::rect(r,0,22*sh/24,sw,sh*2/24+2,mix({54,72,108},{164,199,190},daylight));
  const int sunX=static_cast<int>(sw*.77f+std::sin(yaw)*92.f);
  const int sunY=90+static_cast<int>(25.f*std::sin(time*.013f));
  if(daylight>.32f){
    render::circle(r,sunX,sunY,44,{221,221,176,35});
    render::circle(r,sunX,sunY,26,{255,243,199});
  } else {
    render::circle(r,sunX,sunY,29,{143,171,218});
    render::circle(r,sunX+9,sunY-5,26,{23,36,78});
    for(int i=0;i<35;++i){
      uint32_t h=hxy(i,17,2077);
      int x=static_cast<int>((h%static_cast<uint32_t>(sw)+static_cast<int>(yaw*35.f)+sw*4)%sw);
      int y=20+static_cast<int>((h>>12)%(sh/3));
      render::circle(r,x,y,(h&7u)==0?2:1,{205,223,237});
    }
  }
  // Hazy mountain silhouettes form a distant horizon.
  for(int i=-2;i<9;++i){
    int cx=i*170+static_cast<int>(std::sin(yaw)*45);
    int top=214+(i%3)*19;
    Color c=mix((i%2)?Color{52,67,109}:Color{37,60,98},
                (i%2)?Color{119,160,170}:Color{105,150,167},daylight);
    for(int yy=top;yy<330;++yy){
      int half=(yy-top)*2;
      render::line(r,cx-half,yy,cx+half,yy,c);
    }
  }
}
void terrain(const World& w,Painter& p,bool low,SceneStats& st) {
 const int radius=low?17:25;
 const int cx=static_cast<int>(w.hero.pos.x),cy=static_cast<int>(w.hero.pos.y);
 for(int y=std::max(1,cy-radius);y<std::min(World::H-2,cy+radius);++y){
  for(int x=std::max(1,cx-radius);x<std::min(World::W-2,cx+radius);++x){
    const float dx=x+.5f-w.hero.pos.x,dy=y+.5f-w.hero.pos.y;
    if(dx*dx+dy*dy>radius*radius)continue;
    Tile t=w.tile(x,y);
    uint32_t hv=hxy(x,y,w.seed());
    float h00=w.height(static_cast<float>(x),static_cast<float>(y));
    float h10=w.height(static_cast<float>(x+1),static_cast<float>(y));
    float h01=w.height(static_cast<float>(x),static_cast<float>(y+1));
    float h11=w.height(static_cast<float>(x+1),static_cast<float>(y+1));
    if(t==Tile::Water){
      const float wave=.014f*std::sin(w.time*1.9f+x*.41f+y*.3f);
      h00=h01=h10=h11=.42f+wave;
    }
    Color c=biome(t,hv);
    p.triangle({float(x),float(y),h00},{float(x+1),float(y),h10},
               {float(x+1),float(y+1),h11},c,.97f);
    p.triangle({float(x),float(y),h00},{float(x+1),float(y+1),h11},
               {float(x),float(y+1),h01},c,1.055f);
    ++st.terrainTiles;
    if(t==Tile::Water)continue;
    if(t==Tile::Path && hv%9==0){
      float h=w.height(x+.5f,y+.5f);
      p.quad({x+.13f,y+.35f,h+.018f},{x+.7f,y+.35f,h+.018f},
             {x+.7f,y+.55f,h+.018f},{x+.13f,y+.55f,h+.018f},{194,180,144});
    }
    const legends::TreePosition tree=legends::treeAt(w,x,y);
    if(tree.exists){
      const float px=tree.x,py=tree.y,base=w.height(px,py),scale=tree.scale;
      const float wind=std::sin(w.time*.75f+px*.63f+py*.27f)*.075f;
      p.prism({px,py,base},{px+wind*.5f,py,base+1.65f*scale},.29f*scale,{104,83,63});
      // Three layered canopies sway subtly in the wind.
      const Color leaves=shade({57,148,105},.87f+(hv%14)*.018f);
      p.crystal({px+wind,py,base+1.33f*scale},1.46f*scale,1.31f*scale,leaves);
      p.crystal({px+wind*1.25f,py,base+2.02f*scale},1.18f*scale,1.18f*scale,shade(leaves,1.11f));
      if(!low)p.crystal({px+wind*1.4f,py,base+2.52f*scale},.8f*scale,.83f*scale,shade(leaves,1.21f));
    } else if(t==Tile::Grass&&hv%9==0){
      const float h=w.height(x+.5f,y+.5f);
      p.cone(x+.5f,y+.5f,h,.36f,.31f,(hv%3)?Color{227,220,139}:Color{224,177,190});
    }
    if(!low && (t==Tile::Grass||t==Tile::Forest)&&hv%17==0){
      const float px=x+.62f,py=y+.45f,z=w.height(px,py);
      for(int petal=0;petal<3;++petal){
        float angle=petal*2*kPi/3;
        p.triangle({px,py,z+.18f},
          {px+std::cos(angle)*.17f,py+std::sin(angle)*.17f,z+.32f},
          {px+std::cos(angle+.7f)*.17f,py+std::sin(angle+.7f)*.17f,z+.28f},
          (hv%3)?Color{239,185,197}:Color{241,224,147},1.35f);
      }
    }
  }
 }
}
void shrine(const World& w,Painter& p){
 float x=w.shrine.x,y=w.shrine.y,z=w.height(x,y);
 // Ruined arch in 3D, with an elemental spire hovering over the altar.
 for(int i=-1;i<=1;i+=2){
  const float px=x+i*1.18f;
  p.billboard(px,y,z,0.55f,2.95f,{174,180,163});
  p.billboard(px,y,z+2.85f,.72f,.32f,{214,209,180});
 }
 p.billboard(x,y,z+2.68f,2.85f,.43f,{186,188,170});
 p.billboard(x,y,z,.85f,.8f,{99,124,136});
 float levitate=.2f+std::sin(w.time*2)*.22f;
 Color glow=w.questStage()>=2?Color{120,245,197}:Color{125,213,229};
 p.cone(x,y,z+1.15f+levitate,1.15f,1.85f,glow);
 p.cone(x,y,z+2.70f+levitate,1.04f,-1.22f,{219,246,222});
}
void drawTraveller(Painter& p,float x,float y,float z,float facing,float phase,float gait,
                   bool gliding,bool attacking,float wind,Color tunic,
                   bool runeSword=false,bool forestCharm=false){
  const V3 f={std::cos(facing),std::sin(facing),0};
  const V3 side={-f.y,f.x,0};
  const V3 center{x,y,z};
  const float walk=std::min(1.f,gait/3.3f);
  const float step=std::sin(phase)*.26f*walk;
  // Boots and knees have independent walk cycles; the whole avatar is 3D.
  for(int sign:{-1,1}){
    const V3 hip=center+side*(sign*.23f)+V3{0,0,.83f};
    const V3 foot=center+side*(sign*.23f)+f*(sign*step)+V3{0,0,.12f+std::max(0.f,-sign*step)*.35f};
    p.prism(hip,foot,.25f,{55,65,94});
    p.prism(foot,foot+f*.23f,.27f,{39,49,70});
  }
  p.prism(center+V3{0,0,.81f},center+V3{0,0,1.57f},.80f,tunic);
  // Wind-reactive cape, built from world-space cloth triangles.
  const float flutter=std::sin(wind*1.3f+phase*.23f)*.14f;
  const V3 capeTop=center-f*.33f+V3{0,0,1.53f};
  const V3 capeBot=center-f*(.70f+flutter)+V3{0,0,.55f};
  p.triangle(capeTop+side*.38f,capeTop-side*.38f,capeBot-side*.56f,{46,87,145},1.18f);
  p.triangle(capeTop+side*.38f,capeBot-side*.56f,capeBot+side*.56f,{68,126,186},1.13f);
  for(int sign:{-1,1}){
    const V3 shoulder=center+side*(sign*.48f)+V3{0,0,1.44f};
    const V3 hand=center+side*(sign*.51f)+f*(sign*step*.50f)+V3{0,0,.89f};
    p.prism(shoulder,hand,.23f,{224,182,151});
    if(sign==1 && (attacking||runeSword)){
      const V3 tip=hand+f*(runeSword?1.08f:.75f)+V3{0,0,runeSword?.58f:.4f};
      p.prism(hand,tip,runeSword?.16f:.12f,
              runeSword?Color{138,237,237}:Color{207,224,226},runeSword?2.0f:1.8f);
      p.prism(hand+f*.06f+V3{0,0,.11f},hand-f*.07f+V3{0,0,-.04f},
              .20f,{232,196,135});
      if(runeSword){
        p.crystal(tip,.13f,.16f,{228,255,241},2.1f);
        p.crystal(hand+f*.45f+V3{0,0,.35f+.09f*std::sin(wind*2.f)},
                  .10f,.11f,{130,247,231},1.75f);
      }
    }
  }
  if(forestCharm){
    // A visible green talisman on the chest; not a second character mesh.
    p.crystal(center+f*.42f+V3{0,0,1.30f},.16f,.22f,{135,246,171},2.0f);
    p.crystal(center+f*.48f+V3{0,0,1.32f},.07f,.10f,{234,255,222},2.6f);
  }
  p.prism(center+V3{0,0,1.51f},center+V3{0,0,1.68f},.24f,{222,177,144});
  p.crystal(center+V3{0,0,1.88f},.43f,.34f,{233,195,162},1.24f);
  const V3 hair=center-f*.12f+V3{0,0,1.99f};
  p.crystal(hair,.49f,.19f,{72,58,95});
  if(gliding){
    for(int sign:{-1,1}) {
      V3 root=center+V3{0,0,1.46f};
      V3 tip=root+side*(sign*1.85f)-f*.65f+V3{0,0,.38f};
      V3 low=root+side*(sign*.83f)-f*.75f+V3{0,0,-.18f};
      p.triangle(root,tip,low,{127,216,211},1.6f);
    }
  }
}
void wildlife(const World& w,Painter& p,bool low){
  // Decorative wildlife: deterministic trajectories, not yet full NPC simulation.
  for(int i=0;i<(low?5:13);++i){
    const float ox=31.f+float(i*7%23),oy=30.f+float(i*11%23);
    float t=w.time*.75f+i*2.13f;
    float x=ox+std::sin(t)*1.6f,y=oy+std::cos(t*.83f)*1.4f;
    if(w.tile(static_cast<int>(x),static_cast<int>(y))==Tile::Water)continue;
    float z=w.height(x,y)+1.15f+.31f*std::sin(t*2.8f);
    float wing=.13f+.15f*std::abs(std::sin(t*8.f));
    p.triangle({x,y,z},{x-wing,y-.10f,z+.17f},{x-wing*.7f,y+.08f,z-.10f},
      {247,212,130},1.8f);
    p.triangle({x,y,z},{x+wing,y-.10f,z+.17f},{x+wing*.7f,y+.08f,z-.10f},
      {251,184,166},1.8f);
  }
  if(w.daylight()<.38f){
    for(int i=0;i<(low?8:22);++i){
      const float x=29.f+(i*19%24)+std::sin(w.time*.4f+i)*.3f;
      const float y=30.f+(i*13%23)+std::cos(w.time*.4f+i)*.3f;
      if(w.tile(static_cast<int>(x),static_cast<int>(y))==Tile::Water)continue;
      p.crystal({x,y,w.height(x,y)+.58f+.5f*std::sin(w.time+i)},.09f,.11f,
                {241,238,130},2.4f);
    }
  }
}
void objects(const World& w,Painter& p,float yaw,bool low) {
 shrine(w,p);
 wildlife(w,p,low);
 for(const auto& crystal:w.crystals){
   if(crystal.taken)continue;
   float x=crystal.pos.x,y=crystal.pos.y,z=w.height(x,y)+.5f+std::sin(w.time*2.4f+x)*.17f;
   p.crystal({x,y,z},.83f,1.04f,{98,237,227},1.8f);
 }
 for(const auto& herb:w.herbs){
   if(herb.taken)continue;
   const float x=herb.pos.x,y=herb.pos.y,z=w.height(x,y);
   p.prism({x,y,z+.02f},{x,y,z+.38f},.065f,{88,163,91});
   p.crystal({x-.15f,y,z+.33f},.29f,.21f,{144,223,116});
   p.crystal({x+.14f,y,z+.39f},.27f,.21f,{181,239,133});
   p.crystal({x,y,z+.53f},.18f,.24f,{252,217,132},1.5f);
 }
 for(const auto& e:w.enemies){
   if(!e.alive)continue;
   float x=e.pos.x,y=e.pos.y,z=w.height(x,y)+.11f;
   const Color c=e.hurtPulse>0?Color{255,240,218}:
                  e.slow>0?Color{134,209,223}:Color{209,113,133};
   // Elemental creatures move with a soft squash-and-stretch idle cycle.
   const float bob=.05f*std::sin(w.time*5.f+x);
   p.crystal({x,y,z+.43f+bob},.75f,.55f,c,1.25f);
   p.crystal({x,y,z+.89f+bob},.42f,.23f,shade(c,1.3f),1.3f);
   p.prism({x-.17f,y-.29f,z+.60f+bob},{x-.17f,y-.30f,z+.64f+bob},.12f,{30,47,67});
   p.prism({x+.17f,y-.29f,z+.60f+bob},{x+.17f,y-.30f,z+.64f+bob},.12f,{30,47,67});
   if(e.windup>0) {
     for(int i=0;i<8;++i){
       float a=i*kPi*.25f;
       float r=.77f+.15f*std::sin(w.time*13.f);
       float px=x+std::cos(a)*r,py=y+std::sin(a)*r;
       p.crystal({px,py,w.height(px,py)+.16f},.16f,.28f,{253,96,67},2.4f);
     }
   } else if(e.burn>0) {
     p.crystal({x,y,z+1.4f},.25f,.35f,{248,145,72},1.7f);
   }
   if(e.hp<67.f) {
     p.billboard(x,y,z+1.35f,.9f,.10f,{48,48,56});
     p.billboard(x,y,z+1.36f,.9f*std::clamp(e.hp/90.f,0.f,1.f),.07f,{244,112,126});
   }
 }
 // Explorer's animation derives from actual distance, never just elapsed time.
 const float x=w.hero.pos.x,y=w.hero.pos.y;
 const float ground=w.height(x,y),z=ground+w.hero.air;
 drawTraveller(p,x,y,z,w.hero.facing,w.hero.stridePhase,w.hero.motion,
               w.hero.gliding,w.strikePulse>0,w.time,{70,163,184},
               w.hero.weapon!=0,w.hero.charm!=0);
 // Actual world NPCs: positions and animations are shared with interaction logic.
 for(const auto& n:w.npcs) {
   if(legends::distance(n.pos,w.hero.pos)>19.f)continue;
   drawTraveller(p,n.pos.x,n.pos.y,w.height(n.pos.x,n.pos.y),n.facing,
                 n.stridePhase,n.motion,false,false,w.time,
                 n.kind==0?Color{184,149,97}:Color{153,119,177});
   // A warm quest marker, with a different hue for the healer.
   const float z=w.height(n.pos.x,n.pos.y)+2.45f+.1f*std::sin(w.time*2.2f);
   p.crystal({n.pos.x,n.pos.y,z},.23f,.34f,
             n.kind==0?Color{247,214,112}:Color{175,240,207},1.65f);
 }
 if(w.skillPulse>0){
   Color col{200,200,200};
   switch(w.hero.element){
     case legends::Element::Fire:col={253,158,92};break;
     case legends::Element::Frost:col={155,231,248};break;
     case legends::Element::Wind:col={144,238,181};break;
     case legends::Element::Thunder:col={230,176,250};break;
   }
   for(int i=0;i<12;++i){
     float a=(i*2.f*kPi)/12.f+w.time*1.8f;
     float radius=(1-w.skillPulse/.55f)*w.skillRadius;
     float px=w.pulseAt.x+radius*std::cos(a),py=w.pulseAt.y+radius*std::sin(a);
     p.crystal({px,py,w.height(px,py)+.3f},.24f,.32f,col,2.4f);
   }
 }
 (void)yaw;
}
} // namespace
SceneStats Scene3D::render(SDL_Renderer* renderer,const legends::World& world,float yaw,
                           bool lowDetail,int width,int height) {
 sky(renderer,width,height,yaw,world.daylight(),world.time);
 Camera cam(world,yaw,width,height);
 Painter painter(cam,width,height,world.daylight());
 SceneStats stats;
 terrain(world,painter,lowDetail,stats);
 objects(world,painter,yaw,lowDetail);
 stats.triangles=painter.flush(renderer);
 return stats;
}
} // namespace aetheria
