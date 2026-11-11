// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/BlockMixer.hpp"

#include <algorithm>

#include <cmath>

#include <limits>

#include <stdexcept>

#include <utility>

#include <unordered_map>

#include <vector>

namespace wenuystudio {

namespace {

constexpr std::size_t kMaxBlock=1024;

constexpr double kPi=3.14159265358979323846;

std::uint64_t frames(double seconds,std::uint32_t rate){

 if(!std::isfinite(seconds) || seconds<0 || seconds>20000)

  throw std::invalid_argument("Invalid timeline time");

 return static_cast<std::uint64_t>(std::llround(seconds*rate));

}

float envelope(double local,double duration){

 constexpr double attack=.005,release=.12;

 if(local<0 || local>=duration || duration<=0)return 0.0f;

 return static_cast<float>(std::max(0.0,std::min({1.0,local/attack,(duration-local)/release})));

}

struct PreparedClip {

 float gain=1.0f;

 std::shared_ptr<WaveReader> reader;

 std::uint64_t start=0,offset=0,length=0;

 double sourcePerOutput=1.0;

};

struct PreparedNote {

 std::uint64_t start=0, length=0;

 double angularFrequency=0.0, duration=0.0;

 float velocity=0.0f;

};

struct PreparedTrack {

 float left=1.0f, right=1.0f;

 bool mute=false, solo=false;

 std::vector<PreparedClip> clips;

 std::vector<PreparedNote> notes;

};

}

struct BlockMixer::Impl {

 std::uint32_t rate=48000;

 std::uint64_t total=0;

 float master=1.0f;

 bool hasSolo=false;

 std::vector<PreparedTrack> tracks;

 std::unordered_map<std::string,std::shared_ptr<WaveReader>> openedFiles;

 // Max input/output sample-rate ratio is 192kHz/8kHz = 24.

 std::vector<Stereo> scratch=std::vector<Stereo>(kMaxBlock*24+3);

 explicit Impl(const Project& project) {

  if(project.sampleRate<8000 || project.sampleRate>192000)

   throw std::invalid_argument("Invalid sample rate");

  if(!std::isfinite(project.masterGain) || project.masterGain<0 || project.masterGain>4)

   throw std::invalid_argument("Invalid master gain");

  rate=project.sampleRate;

  total=frames(project.durationSeconds,rate);

  if(total==0)throw std::invalid_argument("Empty project");

  master=project.masterGain;

  if(project.tracks.size()>128)throw std::invalid_argument("Too many tracks");

  tracks.reserve(project.tracks.size());

  for(const auto& track:project.tracks){

   if(!std::isfinite(track.gain)||track.gain<0||track.gain>8||

      !std::isfinite(track.pan)||track.pan< -1||track.pan>1)

    throw std::invalid_argument("Invalid track gain or pan");

   PreparedTrack t;

   t.left=track.gain*(track.pan>0?1-track.pan:1);

   t.right=track.gain*(track.pan<0?1+track.pan:1);

   t.mute=track.mute;t.solo=track.solo;

   if(track.solo && !track.mute)hasSolo=true;

   for(const auto& clip:track.clips){

    if(!std::isfinite(clip.gain)||clip.gain<0||clip.gain>8)

     throw std::invalid_argument("Invalid clip gain");

    PreparedClip c;

    c.gain=clip.gain;

    c.start=frames(clip.startSeconds,rate);

    const auto found=openedFiles.find(clip.filename);

    if(found!=openedFiles.end())c.reader=found->second;

    else{

     auto reader=std::make_shared<WaveReader>(clip.filename);

     openedFiles.emplace(clip.filename,reader);

     c.reader=std::move(reader);

    }

    c.offset=frames(clip.sourceOffsetSeconds,c.reader->rate());

    c.sourcePerOutput=static_cast<double>(c.reader->rate())/rate;

    const auto available=c.offset>=c.reader->frames()?0:c.reader->frames()-c.offset;

    c.length=static_cast<std::uint64_t>(std::ceil(available/c.sourcePerOutput));

    if(clip.durationSeconds!=0)

     c.length=std::min(c.length,frames(clip.durationSeconds,rate));

    // The output duration fits 20k seconds; cap to protect sum calculations.

    if(c.start>=total || c.length==0)continue;

    c.length=std::min(c.length,total-c.start);

    t.clips.emplace_back(std::move(c));

   }

   for(const auto& note:track.notes){

    if(note.key<0||note.key>127||!std::isfinite(note.velocity)||note.velocity<0||note.velocity>1)

     throw std::invalid_argument("Invalid MIDI note");

    PreparedNote n;

    n.start=frames(note.atSeconds,rate);

    n.length=frames(note.durationSeconds,rate);

    if(n.start>=total || n.length==0)continue;

    n.length=std::min(n.length,total-n.start);

    n.angularFrequency=2*kPi*440.0*std::pow(2.0,(note.key-69)/12.0);

    n.duration=note.durationSeconds;

    n.velocity=note.velocity;

    t.notes.emplace_back(n);

   }

   tracks.emplace_back(std::move(t));

  }

 }

};

BlockMixer::BlockMixer(Project project):impl_(std::make_unique<Impl>(project)){}

BlockMixer::~BlockMixer()=default;

BlockMixer::BlockMixer(BlockMixer&&) noexcept=default;

BlockMixer& BlockMixer::operator=(BlockMixer&&) noexcept=default;

std::uint32_t BlockMixer::sampleRate() const noexcept {return impl_->rate;}

std::uint64_t BlockMixer::totalFrames() const noexcept {return impl_->total;}

std::size_t BlockMixer::process(std::uint64_t start,Stereo* dst,std::size_t count,BlockMeter* meter){

 if(count>kMaxBlock || (count && !dst))throw std::invalid_argument("Block buffer must hold 1..1024 frames");

 if(meter)*meter={};

 if(count==0)return 0;

 std::fill_n(dst,count,Stereo{});

 if(start>=impl_->total)return 0;

 count=static_cast<std::size_t>(std::min<std::uint64_t>(count,impl_->total-start));

 for(auto& track:impl_->tracks){

  if(track.mute || (impl_->hasSolo && !track.solo))continue;

  for(auto& clip:track.clips){

   if(start+count<=clip.start || start>=clip.start+clip.length)continue;

   const auto first=std::max(start,clip.start);

   const auto end=std::min(start+count,clip.start+clip.length);

   const auto n=static_cast<std::size_t>(end-first);

   const double sourceStart=clip.offset+(first-clip.start)*clip.sourcePerOutput;

   const auto sourceFirst=static_cast<std::uint64_t>(std::floor(sourceStart));

   const double finalPos=sourceStart+(n-1)*clip.sourcePerOutput;

   const auto need=static_cast<std::size_t>(std::floor(finalPos)-sourceFirst+2);

   if(need>impl_->scratch.size())throw std::runtime_error("Resampling buffer exceeded");

   clip.reader->read(sourceFirst,impl_->scratch.data(),need);

   const auto dest=static_cast<std::size_t>(first-start);

   for(std::size_t i=0;i<n;++i){

    const double sourcePos=sourceStart+i*clip.sourcePerOutput-sourceFirst;

    const auto lo=static_cast<std::size_t>(std::floor(sourcePos));

    const float frac=static_cast<float>(sourcePos-lo);

    const float l=impl_->scratch[lo].l+(impl_->scratch[lo+1].l-impl_->scratch[lo].l)*frac;

    const float r=impl_->scratch[lo].r+(impl_->scratch[lo+1].r-impl_->scratch[lo].r)*frac;

    dst[dest+i].l+=l*clip.gain*track.left;

    dst[dest+i].r+=r*clip.gain*track.right;

   }

  }

  for(const auto& note:track.notes){

   if(start+count<=note.start || start>=note.start+note.length)continue;

   const auto first=std::max(start,note.start);

   const auto end=std::min(start+count,note.start+note.length);

   for(auto f=first;f<end;++f){

    const double sec=double(f-note.start)/impl_->rate;

    const float v=static_cast<float>(std::sin(note.angularFrequency*sec))*note.velocity*.20f*envelope(sec,note.duration);

    const auto i=static_cast<std::size_t>(f-start);

    dst[i].l+=v*track.left;

    dst[i].r+=v*track.right;

   }

  }

 }

 BlockMeter result;

 for(std::size_t i=0;i<count;++i){

  dst[i].l*=impl_->master;

  dst[i].r*=impl_->master;

  result.left=std::max(result.left,std::abs(dst[i].l));

  result.right=std::max(result.right,std::abs(dst[i].r));

 }

 if(meter)*meter=result;

 return count;

}

}
