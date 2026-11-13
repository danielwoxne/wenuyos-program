// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <optional>

namespace wenuystudio {
// Two octave C4..B5 keyboard. Geometry is shared by GTK hit testing and CTest.
inline constexpr int keyboardLowest=60;
inline constexpr int keyboardHighest=83;
inline constexpr int keyboardWhiteCount=14;
inline constexpr double keyboardBlackHeight=.61;
inline constexpr double keyboardBlackWidth=.64;

inline bool keyboardBlack(int key) noexcept {
 if(key<keyboardLowest||key>keyboardHighest)return false;
 const int pc=(key-keyboardLowest)%12;
 return pc==1||pc==3||pc==6||pc==8||pc==10;
}
inline int keyboardWhiteKey(int index) noexcept {
 static constexpr std::array<int,7> pcs{{0,2,4,5,7,9,11}};
 return (index>=0&&index<keyboardWhiteCount)?
  keyboardLowest+(index/7)*12+pcs[static_cast<std::size_t>(index%7)]:-1;
}
inline int keyboardBlackAfter(int whiteIndex) noexcept {
 // A black key overlays the end of these white keys in each octave.
 switch(whiteIndex%7) {
  case 0:return keyboardLowest+(whiteIndex/7)*12+1;
  case 1:return keyboardLowest+(whiteIndex/7)*12+3;
  case 3:return keyboardLowest+(whiteIndex/7)*12+6;
  case 4:return keyboardLowest+(whiteIndex/7)*12+8;
  case 5:return keyboardLowest+(whiteIndex/7)*12+10;
  default:return -1;
 }
}
inline std::optional<int> keyboardKeyAt(double x,double y,double width,double height) noexcept {
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||
    !std::isfinite(height)||width<=0||height<=0||x<0||y<0||
    x>=width||y>=height)return std::nullopt;
 const double white=width/keyboardWhiteCount;
 if(y<height*keyboardBlackHeight){
  for(int i=0;i<keyboardWhiteCount;++i){
   const int key=keyboardBlackAfter(i);
   if(key<0)continue;
   const double left=(i+1-keyboardBlackWidth*.5)*white;
   if(x>=left&&x<left+white*keyboardBlackWidth)return key;
  }
 }
 const int i=static_cast<int>(x/white);
 const int key=keyboardWhiteKey(i);
 return key<0?std::nullopt:std::optional<int>{key};
}
// Optional computer keyboard mapping; lower row C4-B4, upper row C5-B5.
inline int keyboardComputerKey(unsigned keyval) noexcept {
 switch(keyval){
  case 'z':return 60;case 's':return 61;case 'x':return 62;
  case 'd':return 63;case 'c':return 64;case 'v':return 65;
  case 'g':return 66;case 'b':return 67;case 'h':return 68;
  case 'n':return 69;case 'j':return 70;case 'm':return 71;
  case 'q':return 72;case '2':return 73;case 'w':return 74;
  case '3':return 75;case 'e':return 76;case 'r':return 77;
  case '5':return 78;case 't':return 79;case '6':return 80;
  case 'y':return 81;case '7':return 82;case 'u':return 83;
  default:return -1;
 }
}
}
