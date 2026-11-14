// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/DrumGrid.hpp"
#include "wenuystudio/BlockMixer.hpp"
#include "wenuystudio/InstrumentBank.hpp"
#include "wenuystudio/EditModel.hpp"
#include "wenuystudio/ProjectIO.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace wenuystudio;
int main(){
 assert(instrumentCount()==17);
 assert(instrumentById(drumInstrumentId)!=nullptr);
 Project project;project.bpm=120;project.durationSeconds=4;
 project.tracks.emplace_back();
 project.tracks[0].name="Melody";
 project.tracks[0].notes.push_back({60,1,.5,.8f});
 DrumPattern pattern;pattern.bars=4;pattern.swing=.25f;
 EditHistory history;
 assert(history.checkpoint(project));
 const auto result=appendDrumTrack(project,pattern,1.0,"Drum pattern");
 assert(result.trackIndex==1&&result.insertedNotes==56);
 assert(project.tracks.size()==2&&project.tracks[0].notes.size()==1);
 assert(project.tracks[1].instrument==drumInstrumentId);
 assert(project.tracks[1].notes.size()==56);
 assert(project.durationSeconds>=9.2);
 const auto& notes=project.tracks[1].notes;
 assert(notes[0].key==drumKickKey&&std::abs(notes[0].atSeconds-1.)<1e-10);
 bool hasSwing=false,hasSnare=false,hasHat=false;
 for(const auto& n:notes){
  assert(n.durationSeconds==.20);
  assert(n.key==drumKickKey||n.key==drumSnareKey||n.key==drumHiHatKey);
  if(n.key==drumSnareKey)hasSnare=true;
  if(n.key==drumHiHatKey)hasHat=true;
  if(std::abs(n.atSeconds-(1.+.125*5+.03125))<1e-7)hasSwing=true;
 }
 // Default hi-hat pattern contains only even steps; manually test odd-step swing.
 assert(!hasSwing&&hasSnare&&hasHat);
 DrumPattern shifted;shifted.kick=std::uint16_t(1)<<5;
 shifted.snare=0;shifted.hihat=0;shifted.bars=1;shifted.swing=.25f;
 Project odd;odd.durationSeconds=1;odd.bpm=120;
 auto single=appendDrumTrack(odd,shifted,0,"Shifted");
 assert(single.insertedNotes==1);
 assert(std::abs(odd.tracks[0].notes[0].atSeconds-(5*.125+.03125))<1e-8);
 BlockMixer mixer(project);
 std::array<Stereo,512> out{};
 double energy=0;
 for(std::uint64_t f=48000;f<72000;f+=512){
  const auto n=mixer.process(f,out.data(),out.size());
  for(std::size_t i=0;i<n;++i){
   assert(std::isfinite(out[i].l)&&std::isfinite(out[i].r));
   energy+=std::abs(out[i].l)+std::abs(out[i].r);
  }
 }
 assert(energy>1);
 // The same project format and MIDI notes survive save/load.
 const auto path=(std::filesystem::temp_directory_path()/
   "wenuystudio-drummachine-090.wstudio").string();
 try{
  saveProject(project,path);
  auto loaded=loadProject(path);
  assert(loaded.tracks.size()==2&&loaded.tracks[1].instrument==16);
  assert(loaded.tracks[1].notes.size()==56);
  std::filesystem::remove(path);
 }catch(...){std::error_code ec;std::filesystem::remove(path,ec);throw;}
 assert(history.undo(project));
 assert(project.tracks.size()==1&&project.tracks[0].notes.size()==1);
 // Input validation is transactional: invalid data cannot alter the project.
 const auto before=project.tracks.size();
 DrumPattern bad=pattern;bad.swing=1;
 bool rejected=false;
 try{appendDrumTrack(project,bad,0,"Bad");}catch(const std::invalid_argument&){rejected=true;}
 assert(rejected&&project.tracks.size()==before);
 bad=pattern;bad.kick=0;bad.snare=0;bad.hihat=0;
 rejected=false;
 try{appendDrumTrack(project,bad,0,"Bad");}catch(const std::invalid_argument&){rejected=true;}
 assert(rejected&&project.tracks.size()==before);
 project.tracks.resize(128);
 rejected=false;
 try{appendDrumTrack(project,pattern,0,"Too many");}catch(const std::invalid_argument&){rejected=true;}
 assert(rejected&&project.tracks.size()==128);
 std::cout<<"PASS: 16-step drum grid, swing, PCM, save/load, undo and limits\n";
}
