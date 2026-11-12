#include "ScreenshotPixels.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
void test(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main(){
 try{
  const std::array<uint32_t,2> source{{0xff123456u,0x801abcdeu}};
  std::array<uint8_t,12> out{};
  out.fill(0x77u);
  test(aetheria::screenshotPixelsRGBA(source.data(),2,1,out.data(),12),"valid 2 pixels");
  test(out[0]==0x12 && out[1]==0x34 && out[2]==0x56 && out[3]==0xff,
       "ARGB to RGBA conversion");
  test(out[4]==0x1a && out[5]==0xbc && out[6]==0xde && out[7]==0x80,
       "alpha preserved");
  test(out[8]==0x77&&out[9]==0x77,"do not write padding");
  test(!aetheria::screenshotPixelsRGBA(source.data(),2,1,out.data(),7),"reject short stride");
  test(!aetheria::screenshotPixelsRGBA(nullptr,2,1,out.data(),12),"reject missing input");
  test(!aetheria::screenshotPixelsRGBA(source.data(),0,1,out.data(),12),"reject zero width");
  test(!aetheria::screenshotPixelsRGBA(source.data(),2000,1,out.data(),8000),
       "reject overly large allocation");
  std::cout<<"Datatyper screenshot pixel conversion PASS\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
