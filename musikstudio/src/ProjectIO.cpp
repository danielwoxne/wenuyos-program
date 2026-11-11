// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/ProjectIO.hpp"

#include <cmath>

#include <filesystem>

#include <fstream>

#include <iomanip>

#include <limits>

#include <locale>

#include <sstream>

#include <stdexcept>

namespace fs = std::filesystem;

namespace wenuystudio {

namespace {

constexpr std::size_t kMaxProjectBytes=8*1024*1024;

constexpr std::size_t kMaxTracks=128;

constexpr std::size_t kMaxClips=4096;

constexpr std::size_t kMaxNotes=50000;

constexpr std::size_t kMaxPathLength=4096;

void value(bool ok,const char* why) { if(!ok)throw std::runtime_error(why); }

bool safeText(const std::string& s){

 for(unsigned char c:s) if(c==0 || c==10 || c==13) return false;

 return true;

}

void validTime(double t) {value(std::isfinite(t) && t>=0 && t<=20000,"Ogiltig tidsposition i projektet");}

void validate(const Project& p) {

 value(p.sampleRate>=8000 && p.sampleRate<=192000,"Ogiltig samplingsfrekvens");

 validTime(p.durationSeconds);value(p.durationSeconds>0,"Projektet måste ha en längd");

 value(std::isfinite(p.bpm)&&p.bpm>=20&&p.bpm<=400,"Ogiltigt tempo");

 value(p.tracks.size()<=kMaxTracks,"För många spår");

 std::size_t nClips=0,nNotes=0;

 for(const auto& t:p.tracks){

  value(t.name.size()<=512 && safeText(t.name),"Ogiltigt spårnamn");

  value(std::isfinite(t.gain)&&t.gain>=0&&t.gain<=8,"Ogiltig spårvolym");

  value(std::isfinite(t.pan)&&t.pan>=-1&&t.pan<=1,"Ogiltig panorering");

  nClips+=t.clips.size();nNotes+=t.notes.size();

  value(nClips<=kMaxClips && nNotes<=kMaxNotes,"För många klipp eller noter");

  for(const auto& c:t.clips){

   value(!c.filename.empty() && c.filename.size()<=kMaxPathLength && safeText(c.filename),"Ogiltig klippsökväg");

   validTime(c.startSeconds);validTime(c.sourceOffsetSeconds);validTime(c.durationSeconds);

   value(std::isfinite(c.gain)&&c.gain>=0&&c.gain<=8,"Ogiltig klippvolym");

  }

  for(const auto& n:t.notes){

   value(n.key>=0 && n.key<=127,"Ogiltigt MIDI-tonvärde");

   validTime(n.atSeconds);validTime(n.durationSeconds);

   value(std::isfinite(n.velocity)&&n.velocity>=0&&n.velocity<=1,"Ogiltig anslagsstyrka");

  }

 }

}

void noExtra(std::istringstream& is){ is>>std::ws; value(is.eof(),"Extra fält eller felaktig projektfil"); }

std::string nativePath(const std::string& input, const fs::path& base){

 const fs::path p=fs::u8path(input);

 return (p.is_absolute()?p:base/p).lexically_normal().u8string();

}

std::string portablePath(const std::string& input, const fs::path& base){

 const fs::path p=fs::absolute(fs::u8path(input)).lexically_normal();

 std::error_code ec;

 const fs::path relative=fs::relative(p,base,ec);

 return (ec?p:relative).generic_u8string();

}

}

void saveProject(const Project& p,const std::string& filepath){

 validate(p);

 const fs::path path=fs::absolute(fs::u8path(filepath)).lexically_normal();

 value(!path.filename().empty(),"Ogiltig sökväg för projektet");

 std::ostringstream ss;ss.imbue(std::locale::classic());

 ss<<std::setprecision(std::numeric_limits<double>::max_digits10);

 ss<<"WENUYSTUDIO 3\nRATE "<<p.sampleRate<<"\nDURATION "<<p.durationSeconds<<"\nBPM "<<p.bpm<<"\nMASTER "<<p.masterGain<<"\n";

 for(const auto& t:p.tracks){

  ss<<"TRACK "<<std::quoted(t.name)<<' '<<t.gain<<' '<<t.pan<<' '<<(t.mute?1:0)<<' '<<(t.solo?1:0)<<'\n';

  for(const auto& c:t.clips)ss<<"CLIP "<<std::quoted(portablePath(c.filename,path.parent_path()))<<' '<<c.startSeconds<<' '<<c.sourceOffsetSeconds<<' '<<c.durationSeconds<<' '<<c.gain<<'\n';

  for(const auto& n:t.notes)ss<<"NOTE "<<n.key<<' '<<n.atSeconds<<' '<<n.durationSeconds<<' '<<n.velocity<<'\n';

  ss<<"ENDTRACK\n";

 }

 ss<<"END\n";

 const std::string data=ss.str();value(data.size()<=kMaxProjectBytes,"Projektfilen överskrider storleksgränsen");

 const fs::path tmp=path.string()+".part";

 try{

  {

   std::ofstream file(tmp,std::ios::binary|std::ios::trunc);

   value(bool(file),"Kunde inte skapa temporär projektfil");

   file.write(data.data(),static_cast<std::streamsize>(data.size()));file.flush();

   value(bool(file),"Kunde inte skriva projektfilen");

  }

  std::error_code ec;

  if(fs::exists(path)) {

   fs::copy_file(path,path.string()+".bak",fs::copy_options::overwrite_existing,ec);

   value(!ec,"Kunde inte säkerhetskopiera tidigare projektversion");

  }

  fs::rename(tmp,path,ec);

  value(!ec,"Kunde inte slutföra projektsparningen");

 }catch(...){std::error_code ec;fs::remove(tmp,ec);throw;}

}

Project loadProject(const std::string& filepath){

 const fs::path path=fs::absolute(fs::u8path(filepath)).lexically_normal();

 std::error_code ec;

 const auto filesize=fs::file_size(path,ec);

 value(!ec && filesize>0 && filesize<=kMaxProjectBytes,"Projektfil saknas eller är för stor");

 std::ifstream file(path,std::ios::binary);

 value(bool(file),"Kunde inte öppna projektfilen");

 Project p; p.tracks.clear();

 std::string line;bool gotHead=false,gotRate=false,gotDuration=false,gotBpm=false,gotMaster=false,done=false;

 int version=0;

 Track* active=nullptr;std::size_t clips=0,notes=0;

 while(std::getline(file,line)){

  value(line.size()<=kMaxPathLength+128,"För lång rad i projektfilen");

  if(line.empty())continue;

  std::istringstream is(line);is.imbue(std::locale::classic());

  std::string kind;is>>kind;

  if(!gotHead){value(kind=="WENUYSTUDIO","Filen är inte ett Wenuy Studio-projekt");value(bool(is>>version)&&(version==2||version==3),"Projektversionen stöds inte");noExtra(is);gotHead=true;continue;}

  value(!done,"Data efter projektets slut");

  if(kind=="RATE") {value(!active&&!gotRate,"Dubbelt RATE-fält");value(bool(is>>p.sampleRate),"Felaktigt RATE-fält");gotRate=true;}

  else if(kind=="DURATION") {value(!active&&!gotDuration,"Dubbelt DURATION-fält");value(bool(is>>p.durationSeconds),"Felaktigt DURATION-fält");gotDuration=true;}

  else if(kind=="BPM") {value(!active&&!gotBpm,"Dubbelt BPM-fält");value(bool(is>>p.bpm),"Felaktigt BPM-fält");gotBpm=true;}

  else if(kind=="MASTER") {value(version==3 && !active && !gotMaster && gotBpm, "Dubbelt eller felplacerat MASTER-fält");value(bool(is>>p.masterGain),"Felaktigt MASTER-fält");gotMaster=true;}

  else if(kind=="TRACK") {

   value(!active&&gotRate&&gotDuration&&gotBpm&&(version==2||gotMaster),"Felaktig spårordning");

   value(p.tracks.size()<kMaxTracks,"För många spår");

   Track t;int muted=0,solo=0;

   value(bool(is>>std::quoted(t.name)>>t.gain>>t.pan>>muted) && (muted==0||muted==1),"Felaktigt TRACK-fält");

   if(version==3)value(bool(is>>solo)&&(solo==0||solo==1),"Felaktigt solo-fält");

   t.mute=muted!=0;t.solo=solo!=0;p.tracks.push_back(std::move(t));active=&p.tracks.back();

  }

  else if(kind=="CLIP") {

   value(active!=nullptr && ++clips<=kMaxClips,"För många klipp eller klipp utanför spår");

   AudioClip c;std::string src;

   value(bool(is>>std::quoted(src)>>c.startSeconds>>c.sourceOffsetSeconds>>c.durationSeconds>>c.gain),"Felaktigt CLIP-fält");

   value(!src.empty()&&src.size()<=kMaxPathLength && safeText(src),"Felaktig ljudsökväg");

   c.filename=nativePath(src,path.parent_path());active->clips.push_back(std::move(c));

  }

  else if(kind=="NOTE") {

   value(active!=nullptr && ++notes<=kMaxNotes,"För många noter eller noter utanför spår");

   MidiNote n;value(bool(is>>n.key>>n.atSeconds>>n.durationSeconds>>n.velocity),"Felaktigt NOTE-fält");active->notes.push_back(n);

  }

  else if(kind=="ENDTRACK") {value(active!=nullptr,"ENDTRACK utanför spår");active=nullptr;}

  else if(kind=="END") {value(!active&&gotRate&&gotDuration&&gotBpm&&(version==2||gotMaster),"Ofullständigt projekt");done=true;}

  else throw std::runtime_error("Okänt fält i projektfilen: "+kind);

  noExtra(is);

 }

 value(gotHead&&done,"Projektfilen är ofullständig");

 validate(p);return p;

}

}
