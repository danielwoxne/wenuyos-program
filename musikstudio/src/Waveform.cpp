// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Waveform.hpp"

#include "wenuystudio/Wave.hpp"

#include <algorithm>

#include <array>

#include <cstdint>

#include <stdexcept>

namespace wenuystudio {

std::vector<Peak> waveform(const std::string& path,std::size_t columns){

 if(columns==0 || columns>4096)throw std::invalid_argument("Waveform columns must be 1..4096");

 WaveReader r(path);

 std::vector<Peak> out(columns);

 std::array<Stereo,8> samples{};

 for(std::size_t i=0;i<columns;++i){

  const auto begin=static_cast<std::uint64_t>((static_cast<long double>(r.frames())*i)/columns);

  const auto end=static_cast<std::uint64_t>((static_cast<long double>(r.frames())*(i+1))/columns);

  if(end<=begin)continue;

  const std::uint64_t span=end-begin;

  float lo=1.0f,hi=-1.0f;

  const auto add=[&](std::size_t count){

   for(std::size_t j=0;j<count;++j){

    const float a=(samples[j].l+samples[j].r)*0.5f;

    lo=std::min(lo,a);hi=std::max(hi,a);

   }

  };

  if(span<=32){

   for(std::uint64_t offset=0;offset<span;offset+=samples.size()){

    const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(samples.size(),span-offset));

    r.read(begin+offset,samples.data(),count);add(count);

   }

  }else{

   // Four 8-frame windows, spaced across each bucket: only 4 seeks/column.

   for(std::uint64_t window=0;window<4;++window){

    const auto at=begin+window*(span-8)/3;

    r.read(at,samples.data(),samples.size());add(samples.size());

   }

  }

  out[i]={lo,hi};

 }

 return out;

}

}

namespace wenuystudio {

std::vector<Peak> waveformAccurate(const std::string& path,std::size_t columns){

 if(columns==0 || columns>4096)throw std::invalid_argument("Waveform columns must be 1..4096");

 WaveReader reader(path);

 std::vector<Peak> out(columns);

 std::array<Stereo,1024> samples{};

 const auto frames=reader.frames();

 for(std::size_t i=0;i<columns;++i){

  const auto start=static_cast<std::uint64_t>((static_cast<long double>(frames)*i)/columns);

  const auto end=static_cast<std::uint64_t>((static_cast<long double>(frames)*(i+1))/columns);

  if(end<=start)continue;

  float lowest=1.0f,highest=-1.0f;

  for(auto at=start;at<end;){

   const auto n=static_cast<std::size_t>(std::min<std::uint64_t>(samples.size(),end-at));

   reader.read(at,samples.data(),n);

   for(std::size_t j=0;j<n;++j){

    // Preserve transient information on both channels, even when out of phase.

    lowest=std::min({lowest,samples[j].l,samples[j].r});

    highest=std::max({highest,samples[j].l,samples[j].r});

   }

   at+=n;

  }

  out[i]={lowest,highest};

 }

 return out;

}

}
