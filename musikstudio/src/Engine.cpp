// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Engine.hpp"

#include "wenuystudio/BlockMixer.hpp"

#include <algorithm>

#include <array>

#include <cstdio>

#include <stdexcept>

namespace wenuystudio {

void render(const Project& project,const std::string& outputFile,const std::atomic<bool>* keepRunning){

 BlockMixer mixer(project);

 const auto total=mixer.totalFrames();

 if(total>(std::uint64_t(0xffffffff)-36)/4)

  throw std::invalid_argument("Duration does not fit WAV/PCM16; RF64 not implemented");

 constexpr std::size_t block=1024;

 std::array<Stereo,block> mix{};

 const auto tmp=outputFile+".part";

 try{

  {

   WaveWriter out(tmp,project.sampleRate);

   for(std::uint64_t frame=0;frame<total;frame+=block){

    if(keepRunning && !keepRunning->load())throw std::runtime_error("Export cancelled");

    const auto n=mixer.process(frame,mix.data(),static_cast<std::size_t>(std::min<std::uint64_t>(block,total-frame)));

    out.append(mix.data(),n);

   }

   out.finish();

  }

  if(std::rename(tmp.c_str(),outputFile.c_str())!=0)throw std::runtime_error("Could not finalize WAV output");

 }catch(...){std::remove(tmp.c_str());throw;}

}

}
