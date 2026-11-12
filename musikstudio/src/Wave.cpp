// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Wave.hpp"

#include <algorithm>

#include <array>

#include <cmath>

#include <cstring>

#include <limits>

#include <stdexcept>

namespace wenuystudio {

namespace {

uint16_t u16(const unsigned char* b){return uint16_t(b[0])|uint16_t(b[1])<<8;}

uint32_t u32(const unsigned char* b){return uint32_t(u16(b))|uint32_t(u16(b+2))<<16;}

void put16(std::ostream& o,uint16_t x){char b[2]={char(x&255),char(x>>8)};o.write(b,2);}

void put32(std::ostream& o,uint32_t x){put16(o,uint16_t(x&65535));put16(o,uint16_t(x>>16));}

int16_t sample16(float v){return int16_t(std::lround(std::clamp(double(v),-1.0,1.0)*32767.0));}

}

WaveReader::WaveReader(const std::string& path):stream_(path,std::ios::binary){

 if(!stream_)throw std::runtime_error("Cannot open WAV file: "+path);

 unsigned char h[12]{};stream_.read(reinterpret_cast<char*>(h),12);

 if(!stream_||memcmp(h,"RIFF",4)||memcmp(h+8,"WAVE",4))throw std::runtime_error("Not a RIFF/WAVE file");

 bool fmt=false,data=false;

 for(int i=0;i<96&&!data;++i){

  unsigned char ch[8]{};stream_.read(reinterpret_cast<char*>(ch),8);if(!stream_)break;

  uint32_t size=u32(ch+4);

  if(!memcmp(ch,"fmt ",4)){

   if(size<16||size>100000)throw std::runtime_error("Invalid WAV fmt chunk");

   unsigned char f[16]{};stream_.read(reinterpret_cast<char*>(f),16);

   if(!stream_)throw std::runtime_error("Incomplete WAV fmt");

   format_=u16(f);channels_=u16(f+2);rate_=u32(f+4);blockAlign_=u16(f+12);bits_=u16(f+14);

   if((format_!=1&&format_!=3)||channels_<1||channels_>2||rate_<8000||rate_>192000||

       ((format_==1&&bits_!=16&&bits_!=24)||(format_==3&&bits_!=32))||

       blockAlign_!=channels_*bits_/8)throw std::runtime_error("Unsupported WAV encoding");

   stream_.seekg(size-16+(size&1),std::ios::cur);fmt=true;

  }else if(!memcmp(ch,"data",4)){

   if(!fmt)throw std::runtime_error("WAV data before fmt");

   dataOffset_=uint64_t(stream_.tellg());frames_=size/blockAlign_;data=true;

  }else stream_.seekg(uint64_t(size)+(size&1),std::ios::cur);

 }

 if(!fmt||!data)throw std::runtime_error("Missing WAV chunks");

}

void WaveReader::read(uint64_t first,Stereo* out,std::size_t n){

 if(n&& !out)throw std::invalid_argument("Null WAV output");

 std::fill_n(out,n,Stereo{});

 if(first>=frames_||n==0)return;

 const auto available=std::min<uint64_t>(n,frames_-first);

 stream_.clear();stream_.seekg(static_cast<std::streamoff>(dataOffset_+first*blockAlign_),std::ios::beg);

 std::array<unsigned char,8> raw{};

 for(std::uint64_t i=0;i<available;++i){

  stream_.read(reinterpret_cast<char*>(raw.data()),blockAlign_);

  if(!stream_)throw std::runtime_error("WAV file truncated while playing");

  auto value=[&](unsigned channel){const auto* p=raw.data()+channel*(bits_/8);

    if(format_==3){float v=0;memcpy(&v,p,4);return std::isfinite(v)?v:0.f;}

    if(bits_==16)return float(int16_t(u16(p)))/32768.f;

    int32_t a=(int32_t(p[0])|(int32_t(p[1])<<8)|(int32_t(p[2])<<16));

    if(a&0x800000)a|=~0xFFFFFF;

    return float(a)/8388608.f;

  };

  out[i].l=value(0);out[i].r=channels_==2?value(1):out[i].l;

 }

}

WaveWriter::WaveWriter(const std::string& filename,uint32_t sampleRate):stream_(filename,std::ios::binary|std::ios::trunc),rate_(sampleRate){

 if(!stream_||rate_<8000||rate_>192000)throw std::runtime_error("Cannot create WAV output");

 stream_.write("RIFF",4);put32(stream_,0);stream_.write("WAVEfmt ",8);put32(stream_,16);

 put16(stream_,1);put16(stream_,2);put32(stream_,rate_);put32(stream_,rate_*4);put16(stream_,4);put16(stream_,16);

 stream_.write("data",4);put32(stream_,0);

}

WaveWriter::~WaveWriter(){try{finish();}catch(...){}}

void WaveWriter::append(const Stereo* data,std::size_t n){

 if(finished_||(!data&&n))throw std::runtime_error("WAV is closed");

 if(frames_+n>0x3fffffff)throw std::runtime_error("WAV exceeds 4 GB RIFF limit");

 for(std::size_t i=0;i<n;++i){put16(stream_,uint16_t(sample16(data[i].l)));put16(stream_,uint16_t(sample16(data[i].r)));}

 if(!stream_)throw std::runtime_error("WAV write failed");

 frames_+=n;

}

void WaveWriter::finish(){

 if(finished_)return;

 if(!stream_)throw std::runtime_error("WAV file is not writable");

 const auto bytes=uint32_t(frames_*4);stream_.seekp(4);put32(stream_,36+bytes);

 stream_.seekp(40);put32(stream_,bytes);stream_.flush();if(!stream_)throw std::runtime_error("WAV finalize failed");

 finished_=true;stream_.close();

}

}
