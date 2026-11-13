// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/AudioImport.hpp"
#include "wenuystudio/Wave.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace wenuystudio;
int main() {
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/"wenuystudio-audio-import-090";
 fs::create_directories(root);
 try{
  const auto audio=root/"direct.wav";
  {
   WaveWriter w(audio.string(),8000);
   Stereo data[4]{{.1f,.2f},{.3f,.4f},{.1f,.1f},{.0f,.0f}};
   w.append(data,4);w.finish();
  }
  auto result=importAudioFile(audio.string(),(root/"owned").string());
  assert(!result.converted&&result.rate==8000&&result.frames==4);
  assert(result.filename==fs::absolute(audio).string());
  assert(!fs::exists(root/"owned")); // WAV import never allocates or copies
  const auto unknown=root/"not-an-audio-file.bin";
  {
   std::ofstream file(unknown,std::ios::binary);
   file<<"not an audio file";
  }
  bool rejected=false;
  try{(void)importAudioFile(unknown.string(),(root/"owned").string());}
  catch(const std::exception&){rejected=true;}
  assert(rejected);
  rejected=false;
  try{(void)importAudioFile((root/"missing.wav").string(),(root/"owned").string());}
  catch(const std::exception&){rejected=true;}
  assert(rejected);
  fs::remove_all(root);
  std::cout<<"PASS: WAV direct, bounded optional Datatypes, reject invalid/missing audio\n";
 }catch(...){fs::remove_all(root);throw;}
}
