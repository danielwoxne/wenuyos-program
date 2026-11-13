// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/KeyboardInput.hpp"
#include "wenuystudio/MidiTake.hpp"
#include "wenuystudio/ProjectIO.hpp"
#include "wenuystudio/Sequencer.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

int main() {
 using namespace wenuystudio;
 assert(keyboardWhiteCount==14&&keyboardLowest==60&&keyboardHighest==83);
 for(int i=0;i<14;++i){
  const int key=keyboardWhiteKey(i);
  assert(key>=60&&key<=83&&!keyboardBlack(key));
  const double w=560.0/14;
  auto hit=keyboardKeyAt((i+.5)*w,85,560,100);
  assert(hit&&*hit==key);
 }
 const double w=560.0/14;
 assert(keyboardKeyAt((1-.32)*w,20,560,100)==61);
 assert(keyboardKeyAt((2-.32)*w,20,560,100)==63);
 assert(keyboardKeyAt((4-.32)*w,20,560,100)==66);
 assert(keyboardKeyAt((8-.32)*w,20,560,100)==73);
 assert(!keyboardKeyAt(-1,3,560,100));
 assert(!keyboardKeyAt(560,50,560,100));
 assert(!keyboardKeyAt(30,100,560,100));
 assert(!keyboardKeyAt(std::numeric_limits<double>::quiet_NaN(),10,560,100));
 assert(keyboardComputerKey('z')==60&&keyboardComputerKey('u')==83);
 assert(keyboardComputerKey('0')==-1);

 MidiTake take;
 assert(!take.begin(-1,1000000));
 assert(take.begin(2.0,1000000));
 assert(!take.begin(0,1000000));
 assert(!take.noteOn(128,1100000));
 assert(!take.noteOn(60,999999));
 assert(take.noteOn(60,1100000));
 assert(!take.noteOn(60,1150000));
 assert(take.noteOn(64,1150000,.9f)); // a chord
 assert(take.noteOff(64,1350000));
 assert(take.noteOff(60,1500000));
 assert(!take.noteOff(60,1600000));
 assert(take.recorded()==2);
 const auto notes=take.finish(1750000);
 assert(notes.size()==2&&!take.recording());
 assert(notes[0].key==64&&std::abs(notes[0].atSeconds-2.15)<1e-9);
 assert(std::abs(notes[0].durationSeconds-.2)<1e-9);
 assert(notes[1].key==60&&std::abs(notes[1].atSeconds-2.10)<1e-9);
 assert(std::abs(notes[1].durationSeconds-.4)<1e-9);
 assert(take.finish(2000000).empty());

 // Note held when Stop is pressed is finished and retained.
 assert(take.begin(0,2000000));
 assert(take.noteOn(72,2150000));
 auto held=take.finish(2400000);
 assert(held.size()==1&&held[0].key==72);
 assert(std::abs(held[0].durationSeconds-.25)<1e-9);
 assert(!take.noteOn(72,2600000));

 // Imported MIDI take becomes normal editable/exportable notes in .wstudio v5.
 Project p;p.durationSeconds=5;p.tracks.emplace_back();
 for(auto note:notes)assert(insertNote(p,0,note,{4,0}).changed);
 assert(p.tracks[0].notes.size()==2);
 const auto output=(std::filesystem::temp_directory_path()/"wenuystudio-virtual-keyboard-090.wstudio").string();
 try{
  saveProject(p,output);
  const auto restored=loadProject(output);
  assert(restored.tracks.size()==1&&restored.tracks[0].notes.size()==2);
  std::filesystem::remove(output);
 }catch(...){std::filesystem::remove(output);throw;}
 // Every captured event is bounded: never allocate more than 4096 events.
 assert(take.begin(0,0));
 for(int i=0;i<4100;++i){
  const std::int64_t stamp=static_cast<std::int64_t>(i)*10000;
  assert(take.noteOn(60,stamp));
  const bool saved=take.noteOff(60,stamp+1000);
  assert(saved==(i<4096));
 }
 assert(take.finish(50000000).size()==MidiTake::maxTakeNotes);
 std::cout<<"PASS: 2-octave hit testing, polyphonic MIDI capture, stop, limits, project v5\n";
}
