#pragma once
// A tiny declaration-only SDL2 ABI adapter for build systems where the runtime
// library is present but the optional SDL2 development headers are unavailable.
// This is NOT SDL implementation code and adds no third-party source/asset.
// The ABI declarations are specific to SDL2 2.0.x / amd64; verified by a smoke test.
// On a normal Debian 13 developer machine, install libsdl2-dev and use SDL.h.
#include <cstdint>
#ifdef WENUY_USE_SYSTEM_SDL
#include <SDL2/SDL.h>
#else
extern "C" {
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
using SDL_AudioDeviceID = uint32_t;
using SDL_AudioFormat = uint16_t;
using SDL_AudioCallback = void (*)(void*,uint8_t*,int);
struct SDL_AudioSpec {
  int freq; SDL_AudioFormat format; uint8_t channels,silence;
  uint16_t samples,padding; uint32_t size;
  SDL_AudioCallback callback; void* userdata;
};
struct SDL_Rect { int x,y,w,h; };
struct SDL_FPoint {float x,y;};
struct SDL_Color {uint8_t r,g,b,a;};
struct SDL_Vertex { SDL_FPoint position; SDL_Color color; SDL_FPoint tex_coord; };
struct SDL_Keysym { int32_t scancode; int32_t sym; uint16_t mod; uint32_t unused; };
struct SDL_KeyboardEvent { uint32_t type,timestamp,windowID; uint8_t state,repeat,padding2,padding3; SDL_Keysym keysym; };
struct SDL_MouseButtonEvent { uint32_t type,timestamp,windowID,which; uint8_t button,state,clicks,padding1; int32_t x,y; };
union SDL_Event {uint32_t type; SDL_KeyboardEvent key; SDL_MouseButtonEvent button; uint8_t padding[56];};
int SDL_Init(uint32_t flags);
int SDL_InitSubSystem(uint32_t flags);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*,int,const SDL_AudioSpec*,SDL_AudioSpec*,int);
void SDL_CloseAudioDevice(SDL_AudioDeviceID);
void SDL_PauseAudioDevice(SDL_AudioDeviceID,int);
int SDL_QueueAudio(SDL_AudioDeviceID,const void*,uint32_t);
uint32_t SDL_GetQueuedAudioSize(SDL_AudioDeviceID);
void SDL_Quit(void);
const char* SDL_GetError(void);
SDL_Window* SDL_CreateWindow(const char*,int,int,int,int,uint32_t);
void SDL_DestroyWindow(SDL_Window*);
SDL_Renderer* SDL_CreateRenderer(SDL_Window*,int,uint32_t);
void SDL_DestroyRenderer(SDL_Renderer*);
SDL_Texture* SDL_CreateTexture(SDL_Renderer*,uint32_t,int,int,int);
void SDL_DestroyTexture(SDL_Texture*);
int SDL_UpdateTexture(SDL_Texture*,const SDL_Rect*,const void*,int);
int SDL_RenderCopy(SDL_Renderer*,SDL_Texture*,const SDL_Rect*,const SDL_Rect*);
int SDL_RenderSetLogicalSize(SDL_Renderer*,int,int);
int SDL_SetRenderDrawColor(SDL_Renderer*,uint8_t,uint8_t,uint8_t,uint8_t);
int SDL_SetRenderDrawBlendMode(SDL_Renderer*,int);
int SDL_RenderClear(SDL_Renderer*);
int SDL_RenderFillRect(SDL_Renderer*,const SDL_Rect*);
int SDL_RenderDrawRect(SDL_Renderer*,const SDL_Rect*);
int SDL_RenderDrawLine(SDL_Renderer*,int,int,int,int);
int SDL_RenderDrawPoint(SDL_Renderer*,int,int);
int SDL_RenderGeometry(SDL_Renderer*,SDL_Texture*,const SDL_Vertex*,int,const int*,int);
void SDL_RenderPresent(SDL_Renderer*);
int SDL_RenderReadPixels(SDL_Renderer*,const SDL_Rect*,uint32_t,void*,int);
int SDL_PollEvent(SDL_Event*);
void SDL_Delay(uint32_t);
uint32_t SDL_GetTicks(void);
int SDL_SetWindowFullscreen(SDL_Window*,uint32_t);
}
static_assert(sizeof(SDL_Event)==56,"SDL2 event ABI mismatch");
static constexpr uint32_t SDL_INIT_VIDEO=0x00000020u;
static constexpr uint32_t SDL_INIT_AUDIO=0x00000010u;
static constexpr SDL_AudioFormat AUDIO_S16LSB=0x8010u;
static constexpr uint32_t SDL_WINDOW_SHOWN=0x00000004u;
static constexpr uint32_t SDL_WINDOW_RESIZABLE=0x00000020u;
static constexpr uint32_t SDL_WINDOW_FULLSCREEN_DESKTOP=0x00001001u;
static constexpr int SDL_WINDOWPOS_CENTERED=0x2FFF0000;
static constexpr uint32_t SDL_RENDERER_SOFTWARE=0x00000001u;
static constexpr uint32_t SDL_RENDERER_ACCELERATED=0x00000002u;
static constexpr uint32_t SDL_RENDERER_PRESENTVSYNC=0x00000004u;
static constexpr int SDL_TEXTUREACCESS_STREAMING=1;
static constexpr uint32_t SDL_QUIT=0x100u;
static constexpr uint32_t SDL_KEYDOWN=0x300u,SDL_KEYUP=0x301u;
static constexpr uint32_t SDL_MOUSEBUTTONDOWN=0x401u;
static constexpr int SDLK_LEFT=1073741904,SDLK_RIGHT=1073741903;
static constexpr int SDLK_UP=1073741906,SDLK_DOWN=1073741905;
static constexpr int SDLK_LSHIFT=1073742049,SDLK_RSHIFT=1073742053;
static constexpr int SDLK_F2=1073741883;
static constexpr int SDLK_F11=1073741892,SDLK_F5=1073741886,SDLK_F9=1073741890;
static constexpr int SDLK_ESCAPE=27,SDLK_RETURN=13,SDLK_SPACE=32,SDLK_TAB=9;
static constexpr uint32_t SDL_PIXELFORMAT_ARGB8888=0x16362004u;
#endif
