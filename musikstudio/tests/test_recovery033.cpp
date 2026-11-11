// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/ProjectIO.hpp"

#include "wenuystudio/Recovery.hpp"

#include "wenuystudio/MediaCheck.hpp"

#include "wenuystudio/Wave.hpp"

#include <chrono>

#include <filesystem>

#include <stdexcept>

#include <iostream>

#include <fstream>

#include <cstdlib>

#include <unistd.h>

namespace fs=std::filesystem;

using namespace wenuystudio;

static void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}

int main(){

 const auto root=fs::temp_directory_path()/("wenuy-033-"+std::to_string(getpid()));

 try{

  fs::create_directories(root);

  const auto wav=root/"voice.wav", song=root/"song.wstudio";

  {

   WaveWriter output(wav.string(),48000);

   const Stereo one{};

   for(int i=0;i<12;++i)output.append(&one,1);

   output.finish();

  }

  Project p;p.durationSeconds=5;

  Track t;t.name="Voice";t.clips.push_back({wav.string(),0,0,0,1});p.tracks.push_back(t);

  check(inspectMedia(p).empty(),"Valid WAV flagged missing");

  saveProject(p,song.string());

  const auto initial=loadProject(song.string());

  p.tracks[0].name="Changed after crash";

  // Force a guaranteed ordering independent of file-system time resolution.

  fs::last_write_time(song,fs::file_time_type::clock::now()-std::chrono::seconds(10));

  saveRecovery(p,song.string());

  check(fs::exists(recoveryPath(song.string())),"Autosave not created");

  check(hasRecovery(song.string()),"Fresh autosave not detected");

  check(loadProject(song.string()).tracks[0].name==initial.tracks[0].name,"Autosave modified original project");

  check(loadRecovery(song.string()).tracks[0].name=="Changed after crash","Recovery contents mismatch");

  saveRecovery(p,song.string());

  check(fs::exists(recoveryPath(song.string())+".bak"),"Missing autosave backup");

  discardRecovery(song.string());

  check(!fs::exists(recoveryPath(song.string())),"Autosave not discarded");

  check(!fs::exists(recoveryPath(song.string())+".bak"),"Autosave backup not discarded");

  check(fs::exists(song),"Discard removed original");

  // Simulate older recovery than the normal save.

  saveRecovery(p,song.string());

  fs::last_write_time(song,fs::file_time_type::clock::now()+std::chrono::seconds(4));

  check(!hasRecovery(song.string()),"Old autosave was marked fresh");

  discardRecovery(song.string());

  setenv("XDG_STATE_HOME",(root/"state").c_str(),1);

  saveRecovery(p,"");

  check(hasRecovery(""),"Unsaved session autosave missing");

  check(loadRecovery("").tracks[0].name==p.tracks[0].name,"Untitled recovery mismatch");

  discardRecovery("");

  unsetenv("XDG_STATE_HOME");

  // Media check: missing file, broken WAV and truncated clip.

  p.tracks[0].clips[0].filename=(root/"no.wav").string();

  auto issues=inspectMedia(p);

  check(issues.size()==1&&issues[0].kind==MediaIssueKind::Missing,"Missing media not reported");

  const auto malformed=root/"corrupt.wav";

  {std::ofstream f(malformed);f<<"bad";}

  p.tracks[0].clips[0].filename=malformed.string();

  issues=inspectMedia(p);

  check(issues.size()==1&&issues[0].kind==MediaIssueKind::Unreadable,"Corrupt WAV not reported");

  p.tracks[0].clips[0].filename=wav.string();

  p.tracks[0].clips[0].sourceOffsetSeconds=5;

  issues=inspectMedia(p);

  check(issues.size()==1&&issues[0].kind==MediaIssueKind::OutOfRange,"Out-of-range clip not reported");

  fs::remove_all(root);

  std::cout<<"0.3.3 recovery and media tests passed\n";

  return 0;

 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';fs::remove_all(root);return 1;}

}
