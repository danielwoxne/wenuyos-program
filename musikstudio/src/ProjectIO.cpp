// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/ProjectIO.hpp"

#include <cmath>

#include <filesystem>

#include <fstream>

#include <iomanip>

#include <limits>

#include <locale>

#include <sstream>

#include <stdexcept>

namespace wenuystudio {

namespace fs=std::filesystem;

namespace {

void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}

bool sane(const std::string& s){if(s.size()>4096)return false;for(char c:s)if(c=='\r'||c=='\n'||c=='\0')return false;return true;}

void validate(const Project& p){

 require(p.sampleRate>=8000&&p.sampleRate<=192000,"Invalid sample rate");

 require(std::isfinite(p.durationSeconds)&&p.durationSeconds>0&&p.durationSeconds<=20000,"Invalid duration");

 require(std::isfinite(p.bpm)&&p.bpm>=20&&p.bpm<=400,"Invalid tempo");

 require(std::isfinite(p.masterGain)&&p.masterGain>=0&&p.masterGain<=4,"Invalid master volume");

 require(p.tracks.size()<=128,"Too many tracks");

 require(std::isfinite(p.loop.startSeconds)&&std::isfinite(p.loop.endSeconds)&&p.loop.startSeconds>=0&&

         p.loop.endSeconds>=0&&p.loop.startSeconds<=p.durationSeconds&&p.loop.endSeconds<=p.durationSeconds&&

         (!p.loop.enabled||p.loop.endSeconds-p.loop.startSeconds>=.05),"Invalid loop");

 std::size_t clips=0,notes=0;

 for(const auto& t:p.tracks){

  require(sane(t.name)&&t.name.size()<=512,"Invalid track title");

  require(std::isfinite(t.gain)&&t.gain>=0&&t.gain<=8&&std::isfinite(t.pan)&&t.pan>=-1&&t.pan<=1,"Invalid mixer levels");

  require(validTrackEffects(t.effects),"Invalid track FX");

  require(t.instrument>=0&&t.instrument<=3,"Invalid instrument type");

  clips+=t.clips.size();notes+=t.notes.size();require(clips<=4096&&notes<=50000,"Too many events");

  for(const auto& c:t.clips){require(!c.filename.empty()&&sane(c.filename)&&std::isfinite(c.gain)&&c.gain>=0&&c.gain<=8&&

     std::isfinite(c.startSeconds)&&c.startSeconds>=0&&c.startSeconds<=20000&&

     std::isfinite(c.sourceOffsetSeconds)&&c.sourceOffsetSeconds>=0&&c.sourceOffsetSeconds<=20000&&

     std::isfinite(c.durationSeconds)&&c.durationSeconds>=0&&c.durationSeconds<=20000,"Invalid audio clip");}

  for(const auto& n:t.notes){require(n.key>=0&&n.key<=127&&std::isfinite(n.atSeconds)&&n.atSeconds>=0&&n.atSeconds<=20000&&

     std::isfinite(n.durationSeconds)&&n.durationSeconds>=0&&n.durationSeconds<=20000&&

     std::isfinite(n.velocity)&&n.velocity>=0&&n.velocity<=1,"Invalid MIDI note");}

 }

}

std::string portable(const std::string& name,const fs::path& basedir){

 fs::path abs=fs::absolute(fs::u8path(name)).lexically_normal();

 std::error_code ec;auto rel=fs::relative(abs,basedir,ec);return (ec?abs:rel).generic_u8string();

}

void noExtra(std::istringstream& s){s>>std::ws;require(s.eof(),"Extra data in project line");}

}

void saveProject(const Project& p,const std::string& pathstr){

 validate(p);auto path=fs::absolute(fs::u8path(pathstr)).lexically_normal();

 require(!path.filename().empty(),"Missing project filename");

 std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(std::numeric_limits<double>::max_digits10);

 s<<"WENUYSTUDIO 5\nRATE "<<p.sampleRate<<"\nDURATION "<<p.durationSeconds<<"\nBPM "<<p.bpm

   <<"\nMASTER "<<p.masterGain<<"\nLOOP "<<int(p.loop.enabled)<<' '<<p.loop.startSeconds<<' '<<p.loop.endSeconds<<'\n';

 for(const auto& t:p.tracks){

  s<<"TRACK "<<std::quoted(t.name)<<' '<<t.gain<<' '<<t.pan<<' '<<int(t.mute)<<' '<<int(t.solo)<<'\n';

  s<<"INSTR "<<t.instrument<<"\n";

  const auto& f=t.effects;

  s<<"FX "<<int(f.enabled)<<' '<<f.lowDb<<' '<<f.midDb<<' '<<f.highDb<<' '<<f.thresholdDb<<' '<<f.ratio<<'\n';

  for(const auto& c:t.clips)s<<"CLIP "<<std::quoted(portable(c.filename,path.parent_path()))<<' '<<c.startSeconds<<' '

     <<c.sourceOffsetSeconds<<' '<<c.durationSeconds<<' '<<c.gain<<'\n';

  for(const auto& n:t.notes)s<<"NOTE "<<n.key<<' '<<n.atSeconds<<' '<<n.durationSeconds<<' '<<n.velocity<<'\n';

  s<<"ENDTRACK\n";

 }

 s<<"END\n";auto data=s.str();require(data.size()<=8*1024*1024,"Project too large");

 auto tmp=path.string()+".part";auto backup=path.string()+".bak";

 try{

  {std::ofstream file(tmp,std::ios::binary|std::ios::trunc);require(bool(file),"Cannot create temporary project file");

   file.write(data.data(),std::streamsize(data.size()));file.flush();require(bool(file),"Cannot save project");}

  std::error_code ec;

  if(fs::exists(path)){fs::copy_file(path,backup,fs::copy_options::overwrite_existing,ec);require(!ec,"Cannot save project backup");}

  fs::rename(tmp,path,ec);require(!ec,"Cannot finalize project save");

 }catch(...){std::error_code ec;fs::remove(tmp,ec);throw;}

}

Project loadProject(const std::string& pathstr){

 const auto path=fs::absolute(fs::u8path(pathstr)).lexically_normal();

 std::error_code ec;auto size=fs::file_size(path,ec);require(!ec&&size>0&&size<=8*1024*1024,"Missing or oversized project");

 std::ifstream in(path,std::ios::binary);require(bool(in),"Cannot read project");

 Project p;p.tracks.clear();int ver=0;bool header=false,ended=false,rate=false,duration=false,bpm=false,master=false,loop=false,fx=false,instr=false;Track* active=nullptr;

 std::size_t nclips=0,nnotes=0;

 std::string line;

 while(std::getline(in,line)){

  require(line.size()<=4300,"Oversized project record");

  if(line.empty())continue;

  std::istringstream s(line);s.imbue(std::locale::classic());std::string type;s>>type;

  if(!header){require(type=="WENUYSTUDIO"&&bool(s>>ver)&&(ver==2||ver==3||ver==4||ver==5),"Unsupported project header");noExtra(s);header=true;continue;}

  require(!ended,"Unexpected data after end");

  if(type=="RATE"){require(!rate&&!active&&bool(s>>p.sampleRate),"Invalid RATE");rate=true;}

  else if(type=="DURATION"){require(!duration&&!active&&bool(s>>p.durationSeconds),"Invalid DURATION");duration=true;}

  else if(type=="BPM"){require(!bpm&&!active&&bool(s>>p.bpm),"Invalid BPM");bpm=true;}

  else if(type=="MASTER"){require(ver>=3&&!master&&!active&&bool(s>>p.masterGain),"Invalid MASTER");master=true;}

  else if(type=="LOOP"){int e=0;require(ver>=4&&!loop&&!active&&bool(s>>e>>p.loop.startSeconds>>p.loop.endSeconds)&&(e==0||e==1),"Invalid LOOP");p.loop.enabled=e;loop=true;}

  else if(type=="TRACK"){

   require(!active&&rate&&duration&&bpm&&(ver==2||master)&&(ver<4||loop)&&p.tracks.size()<128,"Invalid TRACK order");

   Track t;int mute=0,solo=0;require(bool(s>>std::quoted(t.name)>>t.gain>>t.pan>>mute)&&(mute==0||mute==1),"Invalid TRACK");

   if(ver>=3)require(bool(s>>solo)&&(solo==0||solo==1),"Invalid SOLO");

   t.mute=mute;t.solo=solo;p.tracks.push_back(std::move(t));active=&p.tracks.back();fx=false;instr=false;

  }else if(type=="INSTR"){

   require(ver==5&&active&&!instr,"Invalid INSTR order");

   require(bool(s>>active->instrument)&&active->instrument>=0&&active->instrument<=3,"Invalid instrument");instr=true;

  }else if(type=="FX"){

   require(ver>=4&&active&&!fx&&(ver<5||instr),"Invalid FX order");int enabled=0;auto& f=active->effects;

   require(bool(s>>enabled>>f.lowDb>>f.midDb>>f.highDb>>f.thresholdDb>>f.ratio)&&(enabled==0||enabled==1),"Invalid FX values");

   f.enabled=enabled;fx=true;

  }else if(type=="CLIP"){

   require(active&&++nclips<=4096,"Invalid CLIP order");std::string file;AudioClip c;

   require(bool(s>>std::quoted(file)>>c.startSeconds>>c.sourceOffsetSeconds>>c.durationSeconds>>c.gain)&&!file.empty()&&sane(file),"Invalid CLIP");

   auto src=fs::u8path(file);c.filename=(src.is_absolute()?src:path.parent_path()/src).lexically_normal().u8string();

   active->clips.push_back(std::move(c));

  }else if(type=="NOTE"){

   require(active&&++nnotes<=50000,"Invalid NOTE order");MidiNote n;

   require(bool(s>>n.key>>n.atSeconds>>n.durationSeconds>>n.velocity),"Invalid NOTE");active->notes.push_back(n);

  }else if(type=="ENDTRACK"){

   require(active&&(ver<4||fx)&&(ver<5||instr),"Invalid ENDTRACK");active=nullptr;

  }else if(type=="END"){

   require(!active&&rate&&duration&&bpm&&(ver==2||master)&&(ver<4||loop),"Incomplete project");ended=true;

  }else throw std::runtime_error("Unrecognized project record: "+type);

  noExtra(s);

 }

 require(header&&ended,"Incomplete project");validate(p);return p;

}

}
