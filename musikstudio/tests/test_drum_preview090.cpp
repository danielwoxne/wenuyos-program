// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/DrumPreviewClock.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
using namespace wenuystudio;
int main(){
 DrumPattern p;p.bars=2;p.swing=.25f;
 p.kick=1;p.snare=2;p.hihat=3;
 DrumPreviewClock clock;DrumPreviewClock::Event e{};
 assert(!clock.start(p,0,1000000));
 assert(clock.start(p,120,1000000));
 assert(!clock.popDue(999999,p,e));
 assert(clock.popDue(1000000,p,e));
 assert(e.bar==0&&e.step==0&&e.mask==5&&!e.late);
 assert(clock.delayMillis(1000000)==157);
 assert(!clock.popDue(1156249,p,e));
 assert(clock.popDue(1156250,p,e));
 assert(e.step==1&&e.mask==6&&!e.late);
 p.kick|=std::uint16_t(1)<<2;
 assert(clock.popDue(1250000,p,e)&&e.step==2&&e.mask==1);
 assert(clock.popDue(3000000,p,e)&&e.bar==1&&e.step==0);
 assert(clock.popDue(5000000,p,e)&&e.bar==0&&e.step==0);
 clock.stop();assert(!clock.active()&&!clock.popDue(5200000,p,e));
 p.swing=1;assert(!clock.start(p,120,0));
 p.swing=0;p.bars=0;assert(!clock.start(p,120,0));
 p.bars=4;assert(!clock.start(p,std::numeric_limits<double>::infinity(),0));
 assert(clock.start(p,400,0));
 assert(clock.popDue(0,p,e)&&e.step==0);
 assert(clock.delayMillis(0)==38);
 unsigned count=0;
 for(int i=0;i<8;++i)if(clock.popDue(4000000,p,e))++count;
 assert(count<=2);
 std::cout<<"PASS: drum tempo, swing, loop, edits and skipped backlog\n";
}
