// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Arrangement.hpp"

#include "wenuystudio/TrackFX.hpp"

#include <array>

#include <cassert>

#include <cmath>

#include <stdexcept>

#include <vector>

using namespace wenuystudio;

static void effects(){

 TrackFX dsp;TrackEffects fx;

 dsp.configure(fx,48000);

 std::array<Stereo,128> samples{};

 for(std::size_t i=0;i<samples.size();++i)samples[i]={.125f,.25f};

 dsp.process(samples.data(),samples.size());

 for(const auto& v:samples){assert(v.l==.125f);assert(v.r==.25f);}

 fx.enabled=true;fx.lowDb=6;fx.midDb=-2;fx.highDb=3;fx.ratio=8;fx.thresholdDb=-30;

 dsp.configure(fx,48000);dsp.process(samples.data(),samples.size());

 for(const auto& v:samples){assert(std::isfinite(v.l)&&std::isfinite(v.r));}

 // A split render produces the exact same results as an uninterrupted render.

 std::vector<Stereo> full(1024, Stereo{.50f,.4f});

 auto split=full;TrackFX one,two;one.configure(fx,48000);two.configure(fx,48000);

 one.process(full.data(),full.size());

 two.process(split.data(),100);two.process(split.data()+100,555);two.process(split.data()+655,split.size()-655);

 for(std::size_t i=0;i<full.size();++i){assert(full[i].l==split[i].l);assert(full[i].r==split[i].r);}

 fx.highDb=19;assert(!validTrackEffects(fx));

 try{dsp.configure(fx,48000);assert(false);}catch(const std::invalid_argument&){}

}

static void arrangement(){

 Project p;p.durationSeconds=12;

 Track t;t.name="Guitar";

 t.clips.push_back(AudioClip{"takes/guitar.wav",1,2,6,.8f});

 p.tracks.push_back(t);

 assert(moveAudioClip(p,0,0,2));

 assert(splitAudioClip(p,0,0,4));

 assert(p.tracks[0].clips.size()==2);

 const auto& left=p.tracks[0].clips[0];const auto& right=p.tracks[0].clips[1];

 assert(left.startSeconds==2&&left.durationSeconds==2);

 assert(right.startSeconds==4&&right.sourceOffsetSeconds==4&&right.durationSeconds==4);

 assert(trimAudioClip(p,0,1,5,7));

 assert(p.tracks[0].clips[1].sourceOffsetSeconds==5);

 assert(p.tracks[0].clips[1].durationSeconds==2);

 assert(duplicateAudioClip(p,0,1,9));

 assert(p.tracks[0].clips[2].filename=="takes/guitar.wav");

 assert(!splitAudioClip(p,0,0,2));

 assert(!duplicateAudioClip(p,0,1,11));

 assert(setLoopRegion(p,true,1,5));assert(!setLoopRegion(p,true,1,1.01));

 assert(p.loop.enabled&&p.loop.startSeconds==1&&p.loop.endSeconds==5);

 assert(setLoopRegion(p,false,1,5));

 try{moveAudioClip(p,7,0,0);assert(false);}catch(const std::out_of_range&){}

}

int main(){effects();arrangement();}
