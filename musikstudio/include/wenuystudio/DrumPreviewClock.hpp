// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Sequencer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace wenuystudio {
class DrumPreviewClock final {
public:
 struct Event {unsigned bar=0,step=0,mask=0;bool late=false;};
 bool start(const DrumPattern& pattern,double bpm,std::int64_t nowUs)noexcept{
  stop();
  if(pattern.bars<1||pattern.bars>8||!std::isfinite(pattern.swing)||
     pattern.swing<0||pattern.swing>.45f||!std::isfinite(bpm)||bpm<20||bpm>400)
   return false;
  bars_=pattern.bars;swing_=pattern.swing;
  stepUs_=15000000./bpm;epoch_=nowUs;next_=0;active_=true;return true;
 }
 void stop()noexcept{active_=false;next_=0;}
 bool active()const noexcept{return active_;}
 bool popDue(std::int64_t nowUs,const DrumPattern& pattern,Event& event)noexcept{
  if(!active_||nowUs<epoch_)return false;
  // Skip stale steps after a UI stall; avoid bursts through the audio queue.
  const auto nominal=static_cast<std::uint64_t>(double(nowUs-epoch_)/stepUs_);
  next_=std::max(next_,nominal);
  const double due=at(next_);
  if(double(nowUs)<due)return false;
  event.step=static_cast<unsigned>(next_%16);
  event.bar=static_cast<unsigned>((next_/16)%bars_);
  const auto bit=std::uint16_t(1)<<event.step;
  event.mask=((pattern.kick&bit)?1u:0u)|((pattern.snare&bit)?2u:0u)|
             ((pattern.hihat&bit)?4u:0u);
  event.late=(double(nowUs)-due)>stepUs_*.6;
  ++next_;return true;
 }
 unsigned delayMillis(std::int64_t nowUs)const noexcept{
  if(!active_)return 100;
  const double left=at(next_)-double(nowUs);
  return left<=0?1:static_cast<unsigned>(std::clamp(std::ceil(left/1000.),1.,1000.));
 }
private:
 double at(std::uint64_t n)const noexcept{
  return double(epoch_)+(double(n)+(((n%16)%2)?swing_:0.))*stepUs_;
 }
 std::int64_t epoch_=0;
 std::uint64_t next_=0;
 double stepUs_=125000.;
 float swing_=0;
 unsigned bars_=1;
 bool active_=false;
};
}
