#pragma once
#include "MiniSDL.h"
#include <cstdint>
#include <string>
namespace render {
struct Color { uint8_t r,g,b,a=255; };
void set(SDL_Renderer* r,Color c);
void rect(SDL_Renderer* r,int x,int y,int w,int h,Color c);
void line(SDL_Renderer* r,int x1,int y1,int x2,int y2,Color c);
void circle(SDL_Renderer* r,int x,int y,int radius,Color c);
void text(SDL_Renderer* r,const std::string& s,int x,int y,int scale,Color c,bool shadow=false);
int textWidth(const std::string& s,int scale);
}
