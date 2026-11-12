#include "Font.h"
#include <array>
#include <cctype>
#include <cmath>
#include <unordered_map>
namespace render {
void set(SDL_Renderer* r,Color c) { SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a); }
void rect(SDL_Renderer* r,int x,int y,int w,int h,Color c) {
  if(w<=0||h<=0) return;
  set(r,c); const SDL_Rect box{x,y,w,h}; SDL_RenderFillRect(r,&box);
}
void line(SDL_Renderer* r,int x1,int y1,int x2,int y2,Color c) {
  set(r,c); SDL_RenderDrawLine(r,x1,y1,x2,y2);
}
void circle(SDL_Renderer* r,int x,int y,int radius,Color c) {
  if(radius<1) return;
  set(r,c);
  for(int dy=-radius;dy<=radius;++dy) {
    const int dx=static_cast<int>(std::sqrt(static_cast<float>(radius*radius-dy*dy)));
    SDL_RenderDrawLine(r,x-dx,y+dy,x+dx,y+dy);
  }
}
using Glyph=std::array<uint8_t,7>;
static const std::unordered_map<uint32_t,Glyph> font={
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{31,4,4,4,4,4,31}}, {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {'!',{4,4,4,4,4,0,4}}, {'?',{14,17,1,2,4,0,4}},
 {'.',{0,0,0,0,0,6,6}}, {',',{0,0,0,0,6,6,4}},
 {':',{0,4,4,0,4,4,0}}, {'-',{0,0,0,31,0,0,0}},
 {'+',{0,4,4,31,4,4,0}}, {'/',{1,1,2,4,8,16,16}},
 {'=',{0,31,0,31,0,0,0}}, {'(',{2,4,8,8,8,4,2}},
 {')',{8,4,2,2,2,4,8}}, {'[',{14,8,8,8,8,8,14}},
 {']',{14,2,2,2,2,2,14}}, {'%',{17,2,4,8,17,0,0}},
 {'*',{0,21,14,31,14,21,0}}, {'_',{0,0,0,0,0,0,31}},
 {'\'',{4,4,2,0,0,0,0}}, {'#',{10,31,10,10,31,10,0}},
 {0xC5,{14,0,14,17,31,17,17}}, // Å
 {0xC4,{10,0,14,17,31,17,17}}, // Ä
 {0xD6,{10,0,14,17,17,17,14}}, // Ö
};
static uint32_t nextChar(const std::string& s,size_t& i) {
  uint8_t c=static_cast<uint8_t>(s[i++]);
  if(c<128) return static_cast<uint32_t>(std::toupper(c));
  if((c&0xe0)==0xc0 && i<s.size()) {
    uint8_t c2=static_cast<uint8_t>(s[i++]);
    uint32_t cp=((c&31u)<<6)|(c2&63u);
    if(cp==0xe5) return 0xc5;
    if(cp==0xe4) return 0xc4;
    if(cp==0xf6) return 0xd6;
    return cp;
  }
  return '?';
}
int textWidth(const std::string& s,int scale) {
  int count=0;for(size_t i=0;i<s.size();) {nextChar(s,i);++count;}
  return count*6*scale;
}
void text(SDL_Renderer* r,const std::string& s,int x,int y,int scale,Color c,bool shadow) {
  if(scale<1) return;
  if(shadow) text(r,s,x+scale,y+scale,scale,{8,18,25,180},false);
  set(r,c);
  for(size_t i=0;i<s.size();) {
    const auto ch=nextChar(s,i);
    auto f=font.find(ch);
    if(f!=font.end()) {
      const auto& glyph=f->second;
      for(int gy=0;gy<7;++gy)
        for(int gx=0;gx<5;++gx) if(glyph[gy]&(1u<<(4-gx))) {
          const SDL_Rect box{x+gx*scale,y+gy*scale,scale,scale};SDL_RenderFillRect(r,&box);
        }
    }
    x+=6*scale;
  }
}
} // namespace render
