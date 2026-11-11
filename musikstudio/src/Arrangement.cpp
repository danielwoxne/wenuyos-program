// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Arrangement.hpp"

#include <algorithm>

#include <cmath>

#include <stdexcept>

namespace wenuystudio {

namespace {

void validTime(double t,double limit){if(!std::isfinite(t)||t<0||t>limit)throw std::invalid_argument("Invalid clip time");}

AudioClip& chosen(Project& p,std::size_t t,std::size_t c){

 if(t>=p.tracks.size()||c>=p.tracks[t].clips.size())throw std::out_of_range("Clip not found");

 return p.tracks[t].clips[c];

}

}

bool moveAudioClip(Project& p,std::size_t t,std::size_t c,double start){

 auto& a=chosen(p,t,c);validTime(start,p.durationSeconds);

 if(a.durationSeconds>0&&start+a.durationSeconds>p.durationSeconds)return false;

 if(start==a.startSeconds)return false;

 a.startSeconds=start;return true;

}

bool splitAudioClip(Project& p,std::size_t t,std::size_t c,double at){

 auto& clip=chosen(p,t,c);validTime(at,p.durationSeconds);

 if(!(clip.durationSeconds>0&&at>clip.startSeconds+1e-6&&

     at<clip.startSeconds+clip.durationSeconds-1e-6))return false;

 if(p.tracks[t].clips.size()>=4096)return false;

 const double left=at-clip.startSeconds;

 AudioClip right=clip;

 right.startSeconds=at;right.sourceOffsetSeconds+=left;

 right.durationSeconds-=left;

 clip.durationSeconds=left;

 p.tracks[t].clips.push_back(std::move(right));return true;

}

bool trimAudioClip(Project& p,std::size_t t,std::size_t c,double begin,double end){

 auto& clip=chosen(p,t,c);validTime(begin,p.durationSeconds);validTime(end,p.durationSeconds);

 if(clip.durationSeconds<=0||begin<clip.startSeconds||end>clip.startSeconds+clip.durationSeconds||

   end-begin<.001)return false;

 const double removed=begin-clip.startSeconds;

 clip.startSeconds=begin;clip.sourceOffsetSeconds+=removed;clip.durationSeconds=end-begin;return true;

}

bool duplicateAudioClip(Project& p,std::size_t t,std::size_t c,double start){

 const AudioClip a=chosen(p,t,c);validTime(start,p.durationSeconds);

 if(p.tracks[t].clips.size()>=4096||

   (a.durationSeconds>0&&start+a.durationSeconds>p.durationSeconds))return false;

 auto copy=a;copy.startSeconds=start;p.tracks[t].clips.push_back(copy);return true;

}

bool setLoopRegion(Project& p,bool enabled,double start,double end){

 validTime(start,p.durationSeconds);validTime(end,p.durationSeconds);

 if(enabled&&(end-start)<.05)return false;

 if(p.loop.enabled==enabled && p.loop.startSeconds==start && p.loop.endSeconds==end)return false;

 p.loop={enabled,start,end};return true;

}

}
