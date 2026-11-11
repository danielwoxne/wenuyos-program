// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Wave.hpp"

#include <algorithm>

#include <array>

#include <cmath>

#include <cstring>

#include <limits>

#include <stdexcept>

namespace wenuystudio {

namespace {

std::uint16_t rd16(std::istream& s) {

    unsigned char x[2]; if (!s.read(reinterpret_cast<char*>(x), 2)) throw std::runtime_error("Invalid WAV header");

    return static_cast<std::uint16_t>(x[0] | (x[1]<<8));

}

std::uint32_t rd32(std::istream& s) {

    unsigned char x[4]; if (!s.read(reinterpret_cast<char*>(x), 4)) throw std::runtime_error("Invalid WAV header");

    return std::uint32_t(x[0])|(std::uint32_t(x[1])<<8)|(std::uint32_t(x[2])<<16)|(std::uint32_t(x[3])<<24);

}

void wr16(std::ostream& s, std::uint16_t x) {

    const char c[2]={char(x&255),char((x>>8)&255)};s.write(c,2);

}

void wr32(std::ostream& s, std::uint32_t x) {

    const char c[4]={char(x&255),char((x>>8)&255),char((x>>16)&255),char((x>>24)&255)};s.write(c,4);

}

float sample(const unsigned char* p,std::uint16_t fmt,std::uint16_t bits) {

    if(fmt==1 && bits==16) {

        const std::uint16_t u=std::uint16_t(p[0])|std::uint16_t(std::uint16_t(p[1])<<8);

        const std::int32_t v=(u&0x8000u)?static_cast<std::int32_t>(u)-65536:static_cast<std::int32_t>(u);

        return static_cast<float>(v)/32768.0f;

    }

    if(fmt==3 && bits==32) {

        std::uint32_t u=std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);

        float v=0;std::memcpy(&v,&u,sizeof(v));return std::isfinite(v)?v:0.0f;

    }

    throw std::runtime_error("Unsupported WAV sample format");

}

}

WaveReader::WaveReader(const std::string& path):stream_(path,std::ios::binary) {

    if(!stream_) throw std::runtime_error("Cannot open audio file: "+path);

    char id[4];

    if(!stream_.read(id,4)||std::strncmp(id,"RIFF",4)!=0) throw std::runtime_error("Not a RIFF WAV file");

    const auto riffSize=rd32(stream_);

    if(riffSize<4 || !stream_.read(id,4)||std::strncmp(id,"WAVE",4)!=0) throw std::runtime_error("Not a WAVE file");

    bool gotFmt=false,gotData=false;

    // Stop at RIFF declared boundary; malformed files never allocate according to untrusted lengths.

    const std::uint64_t riffEnd=std::uint64_t(riffSize)+8;

    while(static_cast<std::uint64_t>(stream_.tellg())+8<=riffEnd) {

        if(!stream_.read(id,4))break;

        const auto n=rd32(stream_);

        const auto at=static_cast<std::uint64_t>(stream_.tellg());

        if(at+std::uint64_t(n)>riffEnd)throw std::runtime_error("Truncated WAV chunk");

        if(std::strncmp(id,"fmt ",4)==0 && !gotFmt) {

            if(n<16)throw std::runtime_error("Short WAV format chunk");

            format_=rd16(stream_);channels_=rd16(stream_);rate_=rd32(stream_);

            const auto byteRate=rd32(stream_);const auto align=rd16(stream_);bits_=rd16(stream_);

            const auto bytes=static_cast<std::uint32_t>(channels_)*(bits_/8);

            if(!((format_==1 && bits_==16)||(format_==3 && bits_==32)) || (channels_!=1 && channels_!=2)

               || rate_<8000 || rate_>192000 || align!=bytes || byteRate!=rate_*bytes)

                throw std::runtime_error("Unsupported WAV format (needs PCM16 or float32, mono/stereo)");

            gotFmt=true;

        } else if(std::strncmp(id,"data",4)==0 && !gotData) {

            dataOffset_=at;

            // frames computed after fmt is found, independent of chunk order

            frames_=n;

            gotData=true;

        }

        stream_.seekg(static_cast<std::streamoff>(at+n+(n&1u)),std::ios::beg);

        if(!stream_) throw std::runtime_error("Bad WAV chunk boundary");

        if(gotFmt && gotData)break;

    }

    if(!gotFmt || !gotData)throw std::runtime_error("WAV missing format or data");

    const std::uint64_t bpf=channels_*(bits_/8);

    if(frames_%bpf)throw std::runtime_error("Misaligned WAV data");

    frames_/=bpf;

    stream_.seekg(0,std::ios::end);

    if(!stream_||static_cast<std::uint64_t>(stream_.tellg())<dataOffset_+frames_*bpf)

        throw std::runtime_error("Truncated audio data");

}

void WaveReader::read(std::uint64_t start,Stereo* dst,std::size_t count) {

    if(count && !dst)throw std::invalid_argument("Null audio output buffer");

    if(count==0)return;

    std::fill_n(dst,count,Stereo{});

    if(start>=frames_)return;

    const std::size_t valid=static_cast<std::size_t>(std::min<std::uint64_t>(count,frames_-start));

    const std::size_t bps=bits_/8;

    const std::size_t bpf=bps*channels_;

    stream_.clear();stream_.seekg(static_cast<std::streamoff>(dataOffset_+start*bpf),std::ios::beg);

    constexpr std::size_t kFramesPerRead=1024;

    std::array<unsigned char,8*kFramesPerRead> bytes{};

    for(std::size_t base=0;base<valid;base+=kFramesPerRead){

        const std::size_t n=std::min(kFramesPerRead,valid-base);

        if(!stream_.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(n*bpf)))

            throw std::runtime_error("Cannot read audio data");

        for(std::size_t i=0;i<n;++i){

            const auto* sampleBytes=bytes.data()+i*bpf;

            dst[base+i].l=sample(sampleBytes,format_,bits_);

            dst[base+i].r=channels_==2?sample(sampleBytes+bps,format_,bits_):dst[base+i].l;

        }

    }

}

WaveWriter::WaveWriter(const std::string& path,std::uint32_t sampleRate):stream_(path,std::ios::binary|std::ios::trunc){

    if(!stream_)throw std::runtime_error("Cannot create WAV file: "+path);

    if(sampleRate<8000||sampleRate>192000)throw std::invalid_argument("Unsupported sample rate");

    stream_.write("RIFF",4);wr32(stream_,0);stream_.write("WAVEfmt ",8);wr32(stream_,16);

    wr16(stream_,1);wr16(stream_,2);wr32(stream_,sampleRate);wr32(stream_,sampleRate*4);

    wr16(stream_,4);wr16(stream_,16);stream_.write("data",4);wr32(stream_,0);

}

WaveWriter::~WaveWriter(){try{finish();}catch(...){}}

void WaveWriter::append(const Stereo* frames,std::size_t count){

    if(finished_)throw std::runtime_error("WAV already finished");

    if(count && !frames)throw std::invalid_argument("Null audio input buffer");

    if(count>std::numeric_limits<std::uint32_t>::max()/4-frames_)throw std::runtime_error("WAV exceeds 4 GiB; RF64 planned");

    constexpr std::size_t kWriteFrames=4096;

    std::array<char,kWriteFrames*4> bytes{};

    for(std::size_t base=0;base<count;base+=kWriteFrames){

        const auto n=std::min(kWriteFrames,count-base);

        for(std::size_t i=0;i<n;++i){

            for(std::size_t c=0;c<2;++c){

                const float raw=c==0?frames[base+i].l:frames[base+i].r;

                const float safe=std::isfinite(raw)?std::clamp(raw,-1.0f,1.0f):0.0f;

                const auto v=static_cast<std::int16_t>(std::lrint(safe*32767.0f));

                const auto u=static_cast<std::uint16_t>(v);

                bytes[i*4+c*2]=static_cast<char>(u&255);

                bytes[i*4+c*2+1]=static_cast<char>((u>>8)&255);

            }

        }

        stream_.write(bytes.data(),static_cast<std::streamsize>(n*4));

    }

    if(!stream_)throw std::runtime_error("WAV output failed");

    frames_+=static_cast<std::uint32_t>(count);

}

void WaveWriter::finish(){

    if(finished_)return;

    finished_=true;

    if(!stream_)return;

    stream_.seekp(4,std::ios::beg);wr32(stream_,36+frames_*4);

    stream_.seekp(40,std::ios::beg);wr32(stream_,frames_*4);

    stream_.flush();

    if(!stream_)throw std::runtime_error("Cannot finish WAV output");

}

}
