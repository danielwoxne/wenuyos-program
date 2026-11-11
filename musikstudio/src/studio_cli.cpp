// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Engine.hpp"

#include "wenuystudio/ProjectIO.hpp"

#include "wenuystudio/AudioIO.hpp"

#include "wenuystudio/Waveform.hpp"

#include "wenuystudio/Recovery.hpp"

#include "wenuystudio/MediaCheck.hpp"

#include <filesystem>

#include <chrono>

#include <thread>

#include <csignal>

#include <atomic>

#include <algorithm>

#include <cmath>

#include <exception>

#include <iostream>

#include <string>

#include <utility>

namespace {std::atomic<bool> keepGoing{true};}

extern "C" void onSigInt(int){keepGoing.store(false);}

int main(int argc,char** argv){

 const std::string usage="Användning:\n"

  "  wenuy-studio-cli new PROJEKT.wstudio\n"

  "  wenuy-studio-cli add PROJEKT.wstudio LJUD.wav\n"

  "  wenuy-studio-cli render PROJEKT.wstudio MIX.wav\n"

  "  wenuy-studio-cli info PROJEKT.wstudio\n"

  "  wenuy-studio-cli check PROJEKT.wstudio      # kontrollera ljudfiler\n"

  "  wenuy-studio-cli recovery PROJEKT.wstudio   # visa återställningsstatus\n"

  "  wenuy-studio-cli restore PROJEKT.wstudio NY_FIL.wstudio\n"

  "  wenuy-studio-cli play PROJEKT.wstudio [FRAN_SEKUNDER] [--buffer=low|balanced|safe]\n"

  "  wenuy-studio-cli record FIL.wav [SEKUNDER]\n"

  "  wenuy-studio-cli peaks LJUD.wav\n"

  "  wenuy-studio-cli peaks-accurate LJUD.wav\n"

  "  wenuy-studio-cli devices\n"

  "  wenuy-studio-cli doctor\n"

  "  wenuy-studio-cli probe     # välj aktivt hårdvaruprov (öppnar ljudströmmar)\n";

 try{

  if(argc<2){std::cerr<<usage;return 2;}

  const std::string op=argv[1];

  if(op=="probe"&&argc==2){

   // An opt-in runtime test; no audio or voice is captured/stored.

   const auto p=wenuystudio::probeDefaultAudioDevices();

   std::cout<<"Wenuy Studio 0.4.1 - aktivt ljudenhetsprov\n"

    <<"Högtalare: "<<(p.outputOpened?"ström kan starta":"FEL: "+p.outputError)<<'\n'

    <<"Mikrofon: "<<(p.inputOpened?"ström kan starta":"FEL: "+p.inputError)<<'\n'

    <<"OBS: Detta testar öppning, inte hörbar kvalitet eller verklig inspelning.\n";

   return p.inputOpened&&p.outputOpened?0:3;

  }

  if((op=="devices"||op=="doctor")&&argc==2){

   std::cout<<"Wenuy Studio 0.4.1 - ljuddiagnostik\n";

   try{

    const auto report=wenuystudio::queryAudioDevices();

    std::cout<<"PortAudio: tillgänglig, "<<report.devices.size()<<" ljudenheter\n";

    for(const auto& d:report.devices){

     std::cout<<"  ["<<d.index<<"] "<<d.name<<" ("<<d.hostApi<<")"

      <<" in="<<d.inputChannels<<" ut="<<d.outputChannels

      <<" standard="<<static_cast<long>(d.defaultSampleRate)<<" Hz"

      <<(d.defaultInput?" [standard in]":"")

      <<(d.defaultOutput?" [standard ut]":"")<<'\n';

    }

    if(op=="doctor"){

     if(report.defaultOutput<0)std::cout<<"VARNING: ingen standardutgång för ljud finns.\n";

     if(report.defaultInput<0)std::cout<<"VARNING: ingen standardmikrofon finns.\n";

     std::cout<<"OBS: uppräkning testar inte att fysisk uppspelning eller inspelning fungerar.\n";

     std::cout<<"Prova 'play'/'record' på WenuyOS med riktiga ljudenheter.\n";

    }

    return report.defaultInput>=0&&report.defaultOutput>=0?0:3;

   }catch(const std::exception& e){

    std::cerr<<"Ljuddiagnostik misslyckades: "<<e.what()<<"\n";

    return 1;

   }

  }

  if(argc<3){std::cerr<<usage;return 2;}

  const std::string file=argv[2];

  if(op=="check"&&argc==3){

   const auto p=wenuystudio::loadProject(file);

   const auto issues=wenuystudio::inspectMedia(p);

   if(issues.empty()){std::cout<<"Alla ljudfiler är tillgängliga.\n";return 0;}

   for(const auto& issue:issues)

    std::cout<<wenuystudio::mediaIssueName(issue.kind)<<" spår "<<(issue.trackIndex+1)

     <<" klipp "<<(issue.clipIndex+1)<<": "<<issue.filename<<" – "<<issue.details<<'\n';

   return 3;

  }

  if(op=="recovery"&&argc==3){

   const bool found=wenuystudio::hasRecovery(file);

   std::cout<<(found?"Nyare återställningskopia finns: ":"Ingen nyare återställningskopia finns för ")

    <<(found?wenuystudio::recoveryPath(file):file)<<'\n';

   return found?0:3;

  }

  if(op=="restore"&&argc==4){

   const std::filesystem::path destination=std::filesystem::absolute(argv[3]).lexically_normal();

   if(std::filesystem::exists(destination))throw std::runtime_error("Målet finns redan; återställning får inte skriva över en fil");

   const auto recovered=wenuystudio::loadRecovery(file);

   wenuystudio::saveProject(recovered,destination.string());

   std::cout<<"Återställt till en separat projektfil: "<<destination<<'\n';

   return 0;

  }

  if(op=="new"&&argc==3){

   wenuystudio::Project p;p.durationSeconds=5;wenuystudio::Track empty;empty.name="Spår 1";p.tracks.push_back(std::move(empty));

   wenuystudio::saveProject(p,file);std::cout<<"Skapade "<<file<<'\n';return 0;

  }

  if(op=="add"&&argc==4){

   auto p=wenuystudio::loadProject(file);

   wenuystudio::WaveReader sound(argv[3]);

   wenuystudio::Track track;track.name="Ljudspår "+std::to_string(p.tracks.size()+1);

   track.clips.push_back({argv[3],0.0,0.0,0.0,1.0f});

   p.durationSeconds=std::max(p.durationSeconds,static_cast<double>(sound.frames())/sound.rate());

   p.tracks.push_back(std::move(track));wenuystudio::saveProject(p,file);

   std::cout<<"Importerade ljud till "<<file<<'\n';return 0;

  }

  if(op=="render"&&argc==4){auto p=wenuystudio::loadProject(file);wenuystudio::render(p,argv[3]);std::cout<<"Exporterade "<<argv[3]<<'\n';return 0;}

  if((op=="peaks"||op=="peaks-accurate")&&argc==3){

   auto b=op=="peaks-accurate"?wenuystudio::waveformAccurate(file,64):wenuystudio::waveform(file,64);

   for(const auto& v:b)std::cout<<v.min<<' '<<v.max<<'\n';

   return 0;

  }

  if(op=="play"&&(argc>=3&&argc<=5)){

   const auto project=wenuystudio::loadProject(file);

   double from=0;

   bool suppliedStart=false;

   wenuystudio::BufferPreset preset=wenuystudio::BufferPreset::Balanced;

   for(int i=3;i<argc;++i){

    const std::string arg=argv[i];

    if(arg.rfind("--buffer=",0)==0){

     const auto value=arg.substr(9);

     if(value=="low")preset=wenuystudio::BufferPreset::Low;

     else if(value=="balanced")preset=wenuystudio::BufferPreset::Balanced;

     else if(value=="safe")preset=wenuystudio::BufferPreset::Safe;

     else throw std::invalid_argument("Invalid buffer preset (low, balanced, safe)");

    }else if(!suppliedStart){from=std::stod(arg);suppliedStart=true;}

    else throw std::invalid_argument("Unexpected playback argument");

   }

   wenuystudio::Playback play;

   std::signal(SIGINT,onSigInt);

   play.start(project,from,preset);

   std::cout<<"Spelar upp med buffring (Ctrl+C avbryter)...\n";

   while(keepGoing.load() && play.active())std::this_thread::sleep_for(std::chrono::milliseconds(50));

   play.stop();

   if(!play.error().empty())throw std::runtime_error(play.error());

   const auto d=play.diagnostics();

   std::cout<<"Uppspelning avslutad. Ljudavbrott: "<<d.underruns

    <<", begärd blocktid: "<<d.requestedCallbackMilliseconds<<" ms, "

    <<"buffertkapacitet: "<<d.bufferCapacityFrames<<" ljudbilder.\n";

   if(d.underruns)std::cout<<"TIPS: välj --buffer=safe om ljudet hackar.\n";

   return 0;

  }

  if(op=="record"&&(argc==3||argc==4)){

   const double seconds=argc==4?std::stod(argv[3]):10.0;

   if(!std::isfinite(seconds)||seconds<=0||seconds>3600)throw std::invalid_argument("Invalid recording duration");

   std::signal(SIGINT,onSigInt);

   wenuystudio::Recording rec;rec.start(file,48000);

   std::cout<<"Spelar in mikrofon i "<<seconds<<" sekunder (Ctrl+C stoppar)...\n";

   const auto deadline=std::chrono::steady_clock::now()+std::chrono::duration<double>(seconds);

   while(keepGoing.load()&&rec.active()&&std::chrono::steady_clock::now()<deadline)

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

   rec.stop();if(!rec.error().empty())throw std::runtime_error(rec.error());

   std::cout<<"Inspelade "<<rec.frames()<<" bildrutor till "<<file<<'\n';return 0;

  }

  if(op=="info"&&argc==3){

   auto p=wenuystudio::loadProject(file);

   std::cout<<"Wenuy Studio | "<<p.sampleRate<<" Hz | "<<p.durationSeconds<<" s | "<<p.bpm<<" BPM\n";

   for(const auto& track:p.tracks)std::cout<<"  "<<track.name<<": "<<track.clips.size()<<" ljudklipp, "<<track.notes.size()<<" noter\n";

   return 0;

  }

  std::cerr<<usage;return 2;

 }catch(const std::exception& e){std::cerr<<"Fel: "<<e.what()<<'\n';return 1;}

}
