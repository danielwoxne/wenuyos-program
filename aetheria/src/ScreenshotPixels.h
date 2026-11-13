#pragma once
// Headless, bounded ARGB8888 -> RGBA8888 conversion.
// The game image never exceeds 960x600, but reject malicious dimensions too.
#include <cstddef>
#include <cstdint>
namespace aetheria {
inline bool screenshotPixelsRGBA(const uint32_t* argb,int width,int height,
                                 uint8_t* rgba,size_t stride) noexcept {
 if(!argb||!rgba||width<=0||height<=0||width>1920||height>1080||
    static_cast<size_t>(width)*4u>stride)return false;
 for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
   const uint32_t p=argb[static_cast<size_t>(y)*static_cast<size_t>(width)+x];
   uint8_t* dst=rgba+static_cast<size_t>(y)*stride+static_cast<size_t>(x)*4u;
   dst[0]=static_cast<uint8_t>(p>>16);
   dst[1]=static_cast<uint8_t>(p>>8);
   dst[2]=static_cast<uint8_t>(p);
   dst[3]=static_cast<uint8_t>(p>>24);
 }
 return true;
}
}
