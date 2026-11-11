// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/AudioBuffer.hpp"

#include <algorithm>

#include <cmath>

#include <stdexcept>

namespace wenuystudio {

AudioBuffer::AudioBuffer(std::size_t n):frames_(n),mask_(n-1){

 if(n<512||n>65536||(n&(n-1))!=0)

  throw std::invalid_argument("Audio ring capacity must be a power of two in [512,65536]");

}

std::size_t AudioBuffer::available() const noexcept {

 const auto w=write_.load(std::memory_order_acquire);

 const auto r=read_.load(std::memory_order_acquire);

 return w>=r ? std::min(capacity(),static_cast<std::size_t>(w-r)) : 0;

}

std::size_t AudioBuffer::freeFrames() const noexcept {return capacity()-available();}

std::size_t AudioBuffer::push(const Stereo* src,std::size_t count) noexcept {

 if(!src||count==0)return 0;

 const auto w=write_.load(std::memory_order_relaxed);

 const auto r=read_.load(std::memory_order_acquire);

 const auto n=std::min(count,capacity()-static_cast<std::size_t>(w-r));

 for(std::size_t i=0;i<n;++i)frames_[(static_cast<std::size_t>(w)+i)&mask_]=src[i];

 write_.store(w+n,std::memory_order_release);

 return n;

}

std::size_t AudioBuffer::popInterleaved(float* dst,std::size_t count,

                                         float& peakL,float& peakR) noexcept {

 peakL=0;peakR=0;

 if(!dst||count==0)return 0;

 const auto r=read_.load(std::memory_order_relaxed);

 const auto w=write_.load(std::memory_order_acquire);

 const auto n=std::min(count,static_cast<std::size_t>(w-r));

 for(std::size_t i=0;i<n;++i){

  const auto& sample=frames_[(static_cast<std::size_t>(r)+i)&mask_];

  dst[2*i]=sample.l;dst[2*i+1]=sample.r;

  peakL=std::max(peakL,std::abs(sample.l));

  peakR=std::max(peakR,std::abs(sample.r));

 }

 std::fill(dst+2*n,dst+2*count,0.0f);

 read_.store(r+n,std::memory_order_release);

 return n;

}

}
