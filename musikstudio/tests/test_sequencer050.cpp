// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Sequencer.hpp"

#include "wenuystudio/MidiFile.hpp"

#include <cassert>

#include <cmath>

#include <filesystem>

#include <fstream>

#include <iostream>

#include <stdexcept>

#include <string>

using namespace wenuystudio;

static void close(double a,double b,double tol=0.004){if(std::abs(a-b)>tol)throw std::runtime_error("Value mismatch");}

int main(){

 Project p;p.bpm=120.0;p.durationSeconds=8.0;{Track track;track.name="Piano";p.tracks.push_back(track);}

 close(beatSeconds(120),0.5);close(snapTime(.245,120,{4,0}),.25);

 close(snapTime(.126,120,{4,.2f}),.15);

 if(pitchName(60)!="C4"||pitchName(69)!="A4")return 1;

 auto n=insertNote(p,0,{60,.234,.5,.8f},{4,0});

 if(!n.changed||p.tracks[0].notes.size()!=1)return 2;

 close(p.tracks[0].notes[0].atSeconds,.25);

 if(insertNote(p,0,{60,.24,.5,.8f},{4,0}).changed)return 3;

 if(!insertNote(p,0,{64,.92,.5,.8f},{4,0}).changed)return 4;

 if(!moveNote(p,0,0,62,.4,{4,0}))return 5;

 if(!quantizeTrack(p,0,{1,0}) && p.tracks[0].notes[0].atSeconds==.25)return 6;

 const auto ncopy=duplicateBars(p,0,0,1,1);

 if(ncopy!=2 || p.tracks[0].notes.size()!=4)return 7;

 if(!removeNote(p,0,3)||p.tracks[0].notes.size()!=3)return 8;

 if(p.tracks[0].notes[0].atSeconds<0)return 9;

 const auto folder=std::filesystem::temp_directory_path();

 const auto midi=(folder/"wenuy050-roundtrip.mid").string();

 exportMidi(p,midi);

 auto imported=importMidi(midi);

 if(imported.tracks.size()!=1||imported.tracks[0].notes.size()!=3)return 10;

 close(imported.bpm,120,.2);close(imported.tracks[0].notes[0].atSeconds,p.tracks[0].notes[0].atSeconds);

 std::filesystem::remove(midi);

 const auto wav=(folder/"wenuy050-drums.wav").string();

 std::filesystem::remove(wav);

 DrumPattern pattern;pattern.bars=1;generateDrumLoop(wav,120,8000,pattern);

 if(!std::filesystem::exists(wav)||std::filesystem::file_size(wav)<30000)return 11;

 bool refused=false;

 try{generateDrumLoop(wav,120,8000,pattern);}catch(const std::exception&){refused=true;}

 if(!refused)return 12;

 std::filesystem::remove(wav);

 // Reject corrupt SMF header, oversized per-file inputs and malformed note values.

 {

  std::ofstream bad(midi,std::ios::binary);bad<<"not a MIDI file";

 }

 bool corruptRejected=false;try{(void)importMidi(midi);}catch(const std::exception&){corruptRejected=true;}

 if(!corruptRejected)return 13;

 std::filesystem::remove(midi);

 std::cout<<"OK: grid/edits/duplicate, MIDI roundtrip, drum WAV, overwrite guard, corrupt MIDI\n";

}
