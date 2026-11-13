// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/AudioImport.hpp"
#include "wenuystudio/Wave.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifdef WENUY_STUDIO_DATATYPES
#include <glib.h>
#include <wenuy/WenuyDatatype.h>
#endif

namespace wenuystudio {
namespace fs=std::filesystem;
bool supportsSystemAudioDatatypes() noexcept {
#ifdef WENUY_STUDIO_DATATYPES
 return true;
#else
 return false;
#endif
}
ImportedAudio importAudioFile(const std::string& path,const std::string& ownedMediaDirectory) {
 if(path.empty())throw std::invalid_argument("Empty audio path");
 // A normal supported WAV is streamed directly from its original file.
 // No copy of potentially large audio data is allocated.
 try {
  WaveReader original(path);
  if(!original.frames())throw std::runtime_error("Empty WAV");
  return {fs::absolute(fs::u8path(path)).lexically_normal().u8string(),
          original.rate(),original.frames(),false};
 }catch(const std::exception&){
  // Try system Datatyper only if its C ABI is available in the SDK.
 }
#ifndef WENUY_STUDIO_DATATYPES
 (void)ownedMediaDirectory;
 throw std::runtime_error("Audio format not available; install WenuyOS Datatypes");
#else
 struct AudioData {
  WenuyAudio decoded{};
  ~AudioData(){wenuy_audio_clear(&decoded);}
 } data;
 GError* error=nullptr;
 if(!wenuy_datatype_load_audio(path.c_str(),&data.decoded,&error)){
  std::string message=error?error->message:"Unsupported audio format";
  if(error)g_error_free(error);
  throw std::runtime_error(message);
 }
 if(error)g_error_free(error);
 const auto& a=data.decoded;
 // Hard cap 64 MiB of decoded PCM data (32 million 16-bit values).
 // WenuyOS Datatyper imposes the cap before decoding on 2 GB systems.
 constexpr std::size_t limit=32u*1024u*1024u;
 if(a.rate<8000||a.rate>192000||a.channels<1||a.channels>8||
    !a.frames||!a.samples||a.frames>limit/static_cast<std::size_t>(a.channels))
  throw std::runtime_error("Decoded audio exceeds 64 MiB or is invalid");
 if(ownedMediaDirectory.empty())throw std::runtime_error("No audio import directory");
 fs::create_directories(fs::u8path(ownedMediaDirectory));
 gchar* unique=g_uuid_string_random();
 if(!unique)throw std::runtime_error("Cannot create audio filename");
 const auto output=fs::u8path(ownedMediaDirectory)/(std::string(unique)+".wav");
 g_free(unique);
 if(fs::exists(output))throw std::runtime_error("Duplicate audio filename");
 try {
  WaveWriter wav(output.u8string(),static_cast<std::uint32_t>(a.rate));
  std::array<Stereo,512> block{};
  for(std::size_t position=0;position<a.frames;position+=block.size()) {
   const std::size_t count=std::min(block.size(),a.frames-position);
   for(std::size_t j=0;j<count;++j) {
    const short* samples=a.samples+(position+j)*static_cast<std::size_t>(a.channels);
    float left=0,right=0;int leftCount=0,rightCount=0;
    for(int channel=0;channel<a.channels;++channel) {
     if(channel%2==0){left+=samples[channel]/32768.f;++leftCount;}
     else {right+=samples[channel]/32768.f;++rightCount;}
    }
    left/=leftCount;
    right=rightCount?right/rightCount:left;
    block[j]={left,right};
   }
   wav.append(block.data(),count);
  }
  wav.finish();
 }catch(...){std::error_code ec;fs::remove(output,ec);throw;}
 return {output.u8string(),static_cast<std::uint32_t>(a.rate),
         static_cast<std::uint64_t>(a.frames),true};
#endif
}
}
