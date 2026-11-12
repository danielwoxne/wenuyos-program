// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/BlockMixer.hpp"

#include <algorithm>

#include <array>

#include <cmath>

#include <limits>

#include <stdexcept>

#include <unordered_map>

#include <vector>

namespace wenuystudio {

namespace {

constexpr double pi=3.14159265358979323846;

constexpr std::size_t maxBlock=1024;

std::uint64_t frames(double sec,uint32_t rate){

 if(!std::isfinite(sec)||sec<0||sec>20000)throw std::invalid_argument("Bad position");

 return static_cast<std::uint64_t>(std::llround(sec*rate));

}

struct Clip {float gain=1;std::shared_ptr<WaveReader> file;uint64_t at=0,length=0,offset=0;double ratio=1;};

struct Note {uint64_t at=0,length=0;float velocity=1;double rad=0;};

struct TrackState {

 float left=1,right=1;bool mute=false,solo=false;int instrument=0;TrackFX fx;

 std::vector<Clip> clips;std::vector<Note> notes;

};

}

struct BlockMixer::Impl {

 uint32_t rate=48000;uint64_t length=0;float master=1;bool hasSolo=false;

 std::vector<TrackState> tracks;

 std::unordered_map<std::string,std::shared_ptr<WaveReader>> files;

 std::vector<Stereo> inputScratch=std::vector<Stereo>(maxBlock*24+3);

 std::array<Stereo,maxBlock> trackScratch{};

 uint64_t prev=std::numeric_limits<uint64_t>::max();

 explicit Impl(Project p){

  if(p.sampleRate<8000||p.sampleRate>192000||!std::isfinite(p.masterGain)||p.masterGain<0||p.masterGain>4)

   throw std::invalid_argument("Invalid project settings");

  rate=p.sampleRate;length=frames(p.durationSeconds,rate);master=p.masterGain;

  if(length==0||p.tracks.size()>128)throw std::invalid_argument("Project size invalid");

  tracks.reserve(p.tracks.size());

  for(const auto& t:p.tracks){

   TrackState out;out.left=t.gain*(t.pan>0?1-t.pan:1);

   out.right=t.gain*(t.pan<0?1+t.pan:1);

   out.mute=t.mute;out.solo=t.solo;out.instrument=t.instrument;out.fx.configure(t.effects,rate);

   hasSolo|=(t.solo&&!t.mute);

   for(const auto& c:t.clips){

    auto it=files.find(c.filename);

    if(it==files.end())it=files.emplace(c.filename,std::make_shared<WaveReader>(c.filename)).first;

    Clip x;x.file=it->second;x.at=frames(c.startSeconds,rate);

    x.offset=frames(c.sourceOffsetSeconds,x.file->rate());

    x.ratio=double(x.file->rate())/rate;x.gain=c.gain;

    if(x.offset>=x.file->frames()||x.at>=length)continue;

    x.length=uint64_t(std::ceil((x.file->frames()-x.offset)/x.ratio));

    if(c.durationSeconds>0)x.length=std::min(x.length,frames(c.durationSeconds,rate));

    x.length=std::min(x.length,length-x.at);

    if(x.length)out.clips.emplace_back(std::move(x));

   }

   for(const auto& n:t.notes){

    if(n.key<0||n.key>127||!std::isfinite(n.velocity)||n.velocity<0||n.velocity>1)throw std::invalid_argument("Invalid note");

    Note x;x.at=frames(n.atSeconds,rate);x.length=frames(n.durationSeconds,rate);

    if(x.at>=length||x.length==0)continue;

    x.length=std::min(x.length,length-x.at);

    x.velocity=n.velocity;x.rad=2*pi*440*std::pow(2.,(n.key-69.)/12.);out.notes.push_back(x);

   }

   tracks.emplace_back(std::move(out));

  }

 }

};

BlockMixer::BlockMixer(Project p):p_(std::make_unique<Impl>(std::move(p))){}

BlockMixer::~BlockMixer()=default;

BlockMixer::BlockMixer(BlockMixer&&)noexcept=default;

BlockMixer& BlockMixer::operator=(BlockMixer&&)noexcept=default;

uint32_t BlockMixer::sampleRate()const noexcept{return p_->rate;}

uint64_t BlockMixer::totalFrames()const noexcept{return p_->length;}

std::size_t BlockMixer::process(uint64_t first,Stereo* out,std::size_t count,BlockMeter* meter){

 if(count>maxBlock||(!out&&count))throw std::invalid_argument("Bad audio block");

 if(meter)*meter={};

 if(!count)return 0;

 std::fill_n(out,count,Stereo{});

 if(first>=p_->length)return 0;

 count=static_cast<std::size_t>(std::min<uint64_t>(count,p_->length-first));

 if(first!=p_->prev)for(auto& t:p_->tracks)t.fx.reset();

 p_->prev=first+count;

 for(auto& track:p_->tracks){

  if(track.mute||(p_->hasSolo&&!track.solo))continue;

  auto* scratch=p_->trackScratch.data();std::fill_n(scratch,count,Stereo{});

  for(auto& c:track.clips){

   if(first+count<=c.at||first>=c.at+c.length)continue;

   const auto begin=std::max(first,c.at),end=std::min(first+count,c.at+c.length),n=end-begin;

   const double sourceAt=c.offset+(begin-c.at)*c.ratio;

   const uint64_t start=uint64_t(std::floor(sourceAt));

   const auto required=std::size_t(std::floor(sourceAt+(n-1)*c.ratio)-start+2);

   if(required>p_->inputScratch.size())throw std::runtime_error("Resample buffer exceeded");

   c.file->read(start,p_->inputScratch.data(),required);

   for(uint64_t j=0;j<n;++j){

    const double pos=sourceAt+j*c.ratio-start;

    const auto a=std::size_t(std::floor(pos));const float v=float(pos-a);

    const auto& l=p_->inputScratch[a];const auto& r=p_->inputScratch[a+1];

    auto& dst=scratch[begin-first+j];

    dst.l+=(l.l+(r.l-l.l)*v)*c.gain*track.left;

    dst.r+=(l.r+(r.r-l.r)*v)*c.gain*track.right;

   }

  }

  for(const auto& n:track.notes){

   if(first+count<=n.at||first>=n.at+n.length)continue;

   for(auto f=std::max(first,n.at);f<std::min(first+count,n.at+n.length);++f){

    const double sec=double(f-n.at)/p_->rate;

    const double dur=double(n.length)/p_->rate;

    const float envelope=float(std::clamp(std::min(sec/.006,(dur-sec)/.09),0.,1.));

    const double phase=n.rad*sec;

    double wave=std::sin(phase);

    switch(track.instrument){

     case 1:wave=(2./pi)*std::asin(std::clamp(wave,-1.,1.));break;

     case 2:wave=.7*wave+.25*std::sin(phase*2.)+.15*std::sin(phase*3.);break;

     case 3:wave=.65*wave+.20*std::sin(phase*1.005)+.12*std::sin(phase*2.);break;

     default:break;

    }

    const float sample=float(wave)*.2f*n.velocity*envelope;

    scratch[f-first].l+=sample*track.left;scratch[f-first].r+=sample*track.right;

   }

  }

  track.fx.process(scratch,count);

  for(std::size_t i=0;i<count;++i){out[i].l+=scratch[i].l;out[i].r+=scratch[i].r;}

 }

 for(std::size_t i=0;i<count;++i){

  out[i].l*=p_->master;out[i].r*=p_->master;

  if(meter){meter->left=std::max(meter->left,std::abs(out[i].l));meter->right=std::max(meter->right,std::abs(out[i].r));}

 }

 return count;

}

}
