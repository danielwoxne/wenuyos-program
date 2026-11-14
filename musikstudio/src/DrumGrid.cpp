// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/DrumGrid.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace wenuystudio {
DrumInsertResult appendDrumTrack(Project& p,const DrumPattern& pattern,
                                 double from,const std::string& name) {
 if(!std::isfinite(p.bpm)||p.bpm<20||p.bpm>400||
    !std::isfinite(p.durationSeconds)||p.durationSeconds<=0||p.durationSeconds>20000)
  throw std::invalid_argument("Invalid drum project");
 if(!std::isfinite(from)||from<0||from>=20000||
    pattern.bars<1||pattern.bars>8||
    !std::isfinite(pattern.swing)||pattern.swing<0||pattern.swing>.45f)
  throw std::invalid_argument("Invalid drum pattern");
 if(p.tracks.size()>=128||name.empty()||name.size()>512||
    name.find_first_of("\r\n")!=std::string::npos)
  throw std::invalid_argument("Cannot add drum track");
 const double step=15.0/p.bpm; // one sixteenth-note
 const double barEnd=from+pattern.bars*16.0*step;
 if(!std::isfinite(barEnd)||barEnd+.22>20000)
  throw std::invalid_argument("Drum pattern exceeds maximum duration");
 std::size_t existing=0;
 for(const auto& t:p.tracks)existing+=t.notes.size();
 auto countBits=[](std::uint16_t bits){
  unsigned count=0;
  for(int s=0;s<16;++s)if(bits&(std::uint16_t(1)<<s))++count;
  return count;
 };
 const std::size_t perBar=countBits(pattern.kick)+countBits(pattern.snare)+
                          countBits(pattern.hihat);
 if(!perBar)throw std::invalid_argument("Drum pattern is empty");
 const std::size_t total=perBar*pattern.bars;
 if(total>50000||existing>50000-total)
  throw std::length_error("Too many MIDI notes");
 Track track;
 track.name=name;track.instrument=drumInstrumentId;track.gain=.65f;
 track.notes.reserve(total);
 const struct {std::uint16_t steps;int key;float velocity;} lanes[]={
  {pattern.kick,drumKickKey,.95f},
  {pattern.snare,drumSnareKey,.78f},
  {pattern.hihat,drumHiHatKey,.52f}
 };
 for(unsigned bar=0;bar<pattern.bars;++bar)for(unsigned s=0;s<16;++s){
  const double at=from+(bar*16+s)*step+(s%2?pattern.swing*step:0.0);
  for(const auto& lane:lanes)if(lane.steps&(std::uint16_t(1)<<s)){
   track.notes.push_back({lane.key,at,.20,lane.velocity});
  }
 }
 const std::size_t index=p.tracks.size();
 p.tracks.push_back(std::move(track));
 p.durationSeconds=std::max(p.durationSeconds,barEnd+.22);
 return {index,total,barEnd};
}
}
