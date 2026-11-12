// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Engine.hpp"

#include "wenuystudio/BlockMixer.hpp"

#include <algorithm>

#include <array>

#include <filesystem>

#include <stdexcept>

namespace wenuystudio {

void render(const Project& p,const std::string& dest,const std::atomic<bool>* keepRunning){

 if(std::filesystem::exists(dest))throw std::runtime_error("Output already exists: "+dest);

 const auto tmp=dest+".part";

 try{

  BlockMixer mix(p);WaveWriter out(tmp,p.sampleRate);std::array<Stereo,1024> block{};

  for(uint64_t at=0;at<mix.totalFrames();){

   if(keepRunning&&!keepRunning->load())throw std::runtime_error("Export cancelled");

   auto n=std::size_t(std::min<uint64_t>(1024,mix.totalFrames()-at));

   mix.process(at,block.data(),n);out.append(block.data(),n);at+=n;

  }

  out.finish();std::filesystem::rename(tmp,dest);

 }catch(...){std::error_code e;std::filesystem::remove(tmp,e);throw;}

}

}
