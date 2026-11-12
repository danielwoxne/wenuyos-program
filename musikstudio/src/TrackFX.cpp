// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/TrackFX.hpp"

#include <algorithm>

#include <cmath>

#include <stdexcept>

namespace wenuystudio {

namespace {

constexpr float pi=3.14159265358979323846f;

float amp(float db){return std::pow(10.f,db/20.f);}

float step(float frequency,std::uint32_t rate){return 1.f-std::exp(-2.f*pi*frequency/static_cast<float>(rate));}

}

bool validTrackEffects(const TrackEffects& f) noexcept {

 return std::isfinite(f.lowDb)&&std::isfinite(f.midDb)&&std::isfinite(f.highDb)&&

  std::isfinite(f.thresholdDb)&&std::isfinite(f.ratio)&&

  f.lowDb>=-18.f&&f.lowDb<=18.f&&f.midDb>=-18.f&&f.midDb<=18.f&&

  f.highDb>=-18.f&&f.highDb<=18.f&&f.thresholdDb>=-48.f&&f.thresholdDb<=0.f&&

  f.ratio>=1.f&&f.ratio<=12.f;

}

void TrackFX::configure(const TrackEffects& f,std::uint32_t rate){

 if(!validTrackEffects(f)||rate<8000||rate>192000)throw std::invalid_argument("Invalid track FX settings");

 fx_=f;enabled_=f.enabled;

 lowCoeff_=step(240.f,rate); highCoeff_=step(3800.f,rate);

 lowGain_=amp(f.lowDb);midGain_=amp(f.midDb);highGain_=amp(f.highDb);

 attack_=step(1.f/(.008f*2*pi),rate);release_=step(1.f/(.110f*2*pi),rate);

 threshold_=amp(f.thresholdDb);exponent_=1.f-1.f/f.ratio;

 reset();

}

void TrackFX::reset() noexcept {lowL_=lowR_=highLpL_=highLpR_=env_=0.f;gain_=targetGain_=1.f;decimation_=0;}

void TrackFX::process(Stereo* frames,std::size_t count) noexcept {

 if(!enabled_ || !frames)return;

 for(std::size_t i=0;i<count;++i){

  const float left=frames[i].l,right=frames[i].r;

  lowL_+=lowCoeff_*(left-lowL_);lowR_+=lowCoeff_*(right-lowR_);

  highLpL_+=highCoeff_*(left-highLpL_);highLpR_+=highCoeff_*(right-highLpR_);

  const float highL=left-highLpL_,highR=right-highLpR_;

  float l=lowL_*lowGain_+(highLpL_-lowL_)*midGain_+highL*highGain_;

  float r=lowR_*lowGain_+(highLpR_-lowR_)*midGain_+highR*highGain_;

  float peak=std::max(std::abs(l),std::abs(r));

  env_+=(peak>env_?attack_:release_)*(peak-env_);

  // Compute powf at most once per 16 samples, not on every audio frame.

  // Slew gain at audio rate to avoid 16-frame zipper noise on old CPUs.

  if((decimation_++ & 15u)==0u)

   targetGain_=(env_>threshold_ && exponent_>0.f)

     ?std::pow(threshold_/env_,exponent_):1.f;

  gain_+=.15f*(targetGain_-gain_);

  frames[i]={l*gain_,r*gain_};

 }

}

}
