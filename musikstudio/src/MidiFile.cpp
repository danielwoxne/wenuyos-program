// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/MidiFile.hpp"

#include <algorithm>

#include <array>

#include <cmath>

#include <cstdint>

#include <fstream>

#include <limits>

#include <map>

#include <stdexcept>

#include <string>

#include <utility>

#include <vector>

namespace wenuystudio {

namespace {

void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}

struct Cursor {

 const std::vector<std::uint8_t>& d;std::size_t p=0,limit=0;

 explicit Cursor(const std::vector<std::uint8_t>& v):d(v),limit(v.size()){}

 std::uint8_t one(){check(p<limit,"Truncated MIDI file");return d[p++];}

 std::uint16_t be16(){auto a=one();return static_cast<std::uint16_t>((a<<8)|one());}

 std::uint32_t be32(){std::uint32_t r=0;for(int i=0;i<4;++i)r=(r<<8)|one();return r;}

 std::uint32_t vlq(){std::uint32_t out=0;for(int i=0;i<4;++i){auto c=one();out=(out<<7)|(c&127u);if(!(c&128u))return out;}throw std::runtime_error("MIDI VLQ exceeds 4 bytes");}

 void skip(std::uint32_t n){check(n<=limit-p,"Truncated MIDI event");p+=n;}

};

void append16(std::vector<std::uint8_t>& b,std::uint16_t v){b.push_back(static_cast<std::uint8_t>(v>>8));b.push_back(static_cast<std::uint8_t>(v));}

void append32(std::vector<std::uint8_t>& b,std::uint32_t v){for(int i=24;i>=0;i-=8)b.push_back(static_cast<std::uint8_t>(v>>i));}

void var(std::vector<std::uint8_t>& b,std::uint32_t n){

 std::uint8_t tmp[4];int size=0;tmp[size++]=static_cast<std::uint8_t>(n&127u);

 while((n>>=7)!=0&&size<4)tmp[size++]=static_cast<std::uint8_t>((n&127u)|128u);

 while(size)b.push_back(tmp[--size]);

}

void chunk(std::vector<std::uint8_t>& out,const char name[4],const std::vector<std::uint8_t>& content){

 out.insert(out.end(),name,name+4);check(content.size()<=std::numeric_limits<std::uint32_t>::max(),"MIDI track too large");

 append32(out,static_cast<std::uint32_t>(content.size()));out.insert(out.end(),content.begin(),content.end());

}

struct RawNote {std::uint64_t start=0,end=0;int key=60;float velocity=0.8f;};

struct RawTrack {std::string name;std::vector<RawNote> notes;};

struct Tempo {std::uint64_t tick=0;std::uint32_t micro=500000;};

struct Pending {std::uint64_t start=0;float velocity=0;};

struct Event {std::uint64_t tick;std::vector<std::uint8_t> bytes;};

}

Project importMidi(const std::string& filename){

 std::ifstream file(filename,std::ios::binary|std::ios::ate);check(bool(file),"Cannot open MIDI file");

 const std::streamoff fileLength=file.tellg();check(fileLength>0&&fileLength<=32*1024*1024,"Invalid or too large MIDI file");

 std::vector<std::uint8_t> data(static_cast<std::size_t>(fileLength));file.seekg(0);

 file.read(reinterpret_cast<char*>(data.data()),fileLength);check(bool(file),"Cannot read MIDI file");

 Cursor c(data);

 auto tag=[&](const char* id){for(int i=0;i<4;++i)check(c.one()==static_cast<std::uint8_t>(id[i]),"Invalid MIDI chunk");};

 tag("MThd");auto hlen=c.be32();check(hlen>=6&&hlen<=1024,"Invalid MIDI header length");

 auto type=c.be16(),ntracks=c.be16(),ppq=c.be16();c.skip(hlen-6);

 check(type<=1&&ntracks>=1&&ntracks<=128&&(type!=0||ntracks==1)&&ppq>0&&!(ppq&0x8000u),"Unsupported MIDI format or SMPTE division");

 std::vector<RawTrack> raw;std::vector<Tempo> tempos{{0,500000}};

 std::uint64_t latest=0;std::size_t count=0;

 for(unsigned tr=0;tr<ntracks;++tr){

  tag("MTrk");auto length=c.be32();check(length<=c.limit-c.p,"Truncated MIDI track");

  std::size_t end=c.p+length;

  RawTrack rt;rt.name="MIDI "+std::to_string(tr+1);

  std::array<std::vector<Pending>,16*128> pending{};

  std::uint64_t tick=0;std::uint8_t running=0;

  while(c.p<end){

   c.limit=end;const auto delta=c.vlq();tick+=delta;

   check(tick<=1000000000ULL,"MIDI time is too long");

   std::uint8_t status=c.one(),first=0;bool hasFirst=false;

   if(status<0x80u){check(running>=0x80u&&running<0xF0u,"Invalid MIDI running status");first=status;hasFirst=true;status=running;}

   else if(status<0xF0u)running=status;

   else running=0;

   if(status==0xFFu){

    const auto kind=c.one();const auto len=c.vlq();check(len<=c.limit-c.p,"Truncated MIDI meta event");

    if(kind==0x51u&&len==3){auto micro=(static_cast<unsigned>(c.d[c.p])<<16)|

      (static_cast<unsigned>(c.d[c.p+1])<<8)|c.d[c.p+2];

      check(micro>0,"Invalid MIDI tempo");tempos.push_back({tick,micro});}

    if(kind==0x03u&&len>0&&len<=128){rt.name.assign(reinterpret_cast<const char*>(&c.d[c.p]),len);

      for(char& ch:rt.name){const unsigned char byte=static_cast<unsigned char>(ch);if(byte<32||byte>=127)ch='?';}}

    c.skip(len);if(kind==0x2Fu)break;

    continue;

   }

   if(status==0xF0u||status==0xF7u){c.skip(c.vlq());continue;}

   check(status>=0x80u&&status<=0xEFu,"Unsupported MIDI event");

   const auto command=status&0xF0u,ch=status&0x0Fu;

   std::uint8_t a=hasFirst?first:c.one();std::uint8_t b=0;

   if(command!=0xC0u&&command!=0xD0u)b=c.one();

   check(a<=127&&b<=127,"Invalid MIDI data byte");

   if(command==0x90u&&b>0){pending[ch*128+a].push_back({tick,b/127.0f});}

   else if(command==0x80u||(command==0x90u&&b==0)){

    auto& stack=pending[ch*128+a];if(!stack.empty()){

     const auto start=stack.front();stack.erase(stack.begin());

     if(tick>start.start){check(count<50000,"Too many MIDI notes");rt.notes.push_back({start.start,tick,a,start.velocity});++count;}

    }

   }

  }

  c.p=end;c.limit=data.size();

  for(unsigned k=0;k<pending.size();++k)for(const auto& p:pending[k]){

   if(tick>p.start){check(count<50000,"Too many MIDI notes");rt.notes.push_back({p.start,tick,static_cast<int>(k%128),p.velocity});++count;}

  }

  latest=std::max(latest,tick);raw.push_back(std::move(rt));

 }

 check(count<=50000,"Too many MIDI notes");

 std::stable_sort(tempos.begin(),tempos.end(),[](auto&a,auto&b){return a.tick<b.tick;});

 // Last tempo event at the same tick wins; all note times use the complete tempo map.

 auto toSec=[&](std::uint64_t tick){

  double sec=0;std::uint64_t from=0;std::uint32_t micro=500000;

  for(const auto& t:tempos){if(t.tick>tick)break;

   sec+=(t.tick-from)*micro/(1000000.0*ppq);from=t.tick;micro=t.micro;}

  return sec+(tick-from)*micro/(1000000.0*ppq);

 };

 Project p;p.sampleRate=48000;p.tracks.clear();p.masterGain=1.0f;

 p.bpm=60000000.0/tempos.front().micro;

 for(const auto& t:tempos)if(t.tick==0)p.bpm=60000000.0/t.micro;

 p.bpm=std::clamp(p.bpm,20.0,400.0);

 double farthest=toSec(latest);

 for(const auto& tr:raw){

  if(tr.notes.empty())continue;

  Track track;track.name=tr.name;

  for(const auto& n:tr.notes){const double start=toSec(n.start),end=toSec(n.end);

   if(end<=start||start>=19999)continue;

   track.notes.push_back({n.key,start,std::min(end-start,19999.0-start),n.velocity});

   farthest=std::max(farthest,end);

  }

  if(!track.notes.empty())p.tracks.push_back(std::move(track));

 }

 if(p.tracks.empty()){Track track;track.name="MIDI";p.tracks.push_back(std::move(track));}

 p.durationSeconds=std::clamp(farthest+1.0,1.0,20000.0);

 return p;

}

void exportMidi(const Project& p,const std::string& filename){

 check(std::isfinite(p.bpm)&&p.bpm>=20&&p.bpm<=400,"Invalid BPM");

 check(p.tracks.size()<=128,"Too many tracks");

 constexpr std::uint16_t ppq=480;

 const double ticksPerSecond=ppq*p.bpm/60.0;

 std::vector<std::uint8_t> output;

 output.insert(output.end(),{'M','T','h','d'});append32(output,6);

 append16(output,1);append16(output,static_cast<std::uint16_t>(std::max<std::size_t>(1,p.tracks.size())));append16(output,ppq);

 const auto micro=static_cast<std::uint32_t>(std::llround(60000000.0/p.bpm));

 for(std::size_t ti=0;ti<std::max<std::size_t>(1,p.tracks.size());++ti){

  std::vector<Event> ev;

  if(ti==0)ev.push_back({0,{0xFF,0x51,0x03,static_cast<std::uint8_t>(micro>>16),

     static_cast<std::uint8_t>(micro>>8),static_cast<std::uint8_t>(micro)}});

  if(ti<p.tracks.size()){

   const auto& tr=p.tracks[ti];

   check(tr.notes.size()<=50000,"Too many notes");

   std::string name=tr.name.substr(0,120);

   std::vector<std::uint8_t> title={0xFF,0x03,static_cast<std::uint8_t>(name.size())};

   title.insert(title.end(),name.begin(),name.end());ev.push_back({0,std::move(title)});

   for(const auto& note:tr.notes){

    check(note.key>=0&&note.key<=127&&std::isfinite(note.atSeconds)&&note.atSeconds>=0&&

      std::isfinite(note.durationSeconds)&&note.durationSeconds>0&&

      std::isfinite(note.velocity)&&note.velocity>=0&&note.velocity<=1,"Invalid note for MIDI export");

    auto start=static_cast<std::uint64_t>(std::llround(note.atSeconds*ticksPerSecond));

    auto end=static_cast<std::uint64_t>(std::llround((note.atSeconds+note.durationSeconds)*ticksPerSecond));

    end=std::max(start+1,end);

    check(end<1000000000ULL,"MIDI timestamp out of range");

    std::uint8_t key=static_cast<std::uint8_t>(note.key),velocity=static_cast<std::uint8_t>(std::clamp<int>(

       static_cast<int>(std::lround(note.velocity*127)),1,127));

    ev.push_back({start,{0x90,key,velocity}});

    ev.push_back({end,{0x80,key,0}});

   }

  }

  std::stable_sort(ev.begin(),ev.end(),[](const Event& a,const Event& b){

    if(a.tick!=b.tick)return a.tick<b.tick;

    // Note-off before note-on at the same tick prevents stuck notes.

    const bool offa=!a.bytes.empty()&&a.bytes[0]==0x80;

    const bool offb=!b.bytes.empty()&&b.bytes[0]==0x80;

    return offa&&!offb;

  });

  std::vector<std::uint8_t> body;std::uint64_t prev=0;

  for(const auto& event:ev){check(event.tick>=prev&&event.tick-prev<=0x0fffffff,"MIDI delta out of range");

   var(body,static_cast<std::uint32_t>(event.tick-prev));

   body.insert(body.end(),event.bytes.begin(),event.bytes.end());prev=event.tick;

  }

  body.insert(body.end(),{0,0xff,0x2f,0});chunk(output,"MTrk",body);

 }

 std::ofstream f(filename,std::ios::binary|std::ios::trunc);

 check(bool(f),"Cannot write MIDI file");

 f.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(output.size()));

 check(bool(f),"Cannot finish MIDI file");

}

}
