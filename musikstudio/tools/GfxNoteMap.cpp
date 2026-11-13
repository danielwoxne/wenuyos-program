// SPDX-License-Identifier: GPL-3.0-or-later
// Optional standalone 3D note map. The normal GTK music window does not link
// against WenuyGFX, so weak or driverless computers retain their fast UI.
#include "wenuystudio/ProjectIO.hpp"
#include <wenuygfx/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {
bool sv() {
 const char* s=std::getenv("LANGUAGE");
 if(!s||!*s)s=std::getenv("LANG");
 return s&&std::strncmp(s,"sv",2)==0;
}
const char* tr(const char* en,const char* se){return sv()?se:en;}
wenuygfx::Scene noteScene(const wenuystudio::Project& project) {
 wenuygfx::Scene scene;
 const std::size_t tracks=std::min<std::size_t>(16,project.tracks.size());
 const double duration=std::max(1.,project.durationSeconds);
 wenuygfx::addQuad(scene,{-8.5f,-4.5f,-.12f},{8.5f,-4.5f,-.12f},
                   {8.5f,4.5f,-.12f},{-8.5f,4.5f,-.12f},
                   {112,121,135,255},wenuygfx::Material::Flat);
 std::size_t displayed=0;
 for(std::size_t track=0;track<tracks&&displayed<256;++track) {
  const auto& notes=project.tracks[track].notes;
  const float y=-4.f+(track+.5f)*(8.f/std::max<std::size_t>(tracks,1));
  for(const auto& note:notes) {
   if(displayed>=256)break;
   if(!std::isfinite(note.atSeconds)||!std::isfinite(note.durationSeconds)||
      note.atSeconds>=duration||note.durationSeconds<=0||note.key<0||note.key>127)continue;
   const float at=static_cast<float>(note.atSeconds/duration*16-8);
   const float length=std::clamp(static_cast<float>(note.durationSeconds/duration*16),.07f,1.6f);
   const float height=.18f+std::clamp((note.key-36)/48.f,0.f,1.f)*3.8f;
   const unsigned shade=static_cast<unsigned>(80+std::clamp(note.velocity,0.f,1.f)*145);
   const wenuygfx::Color color{
     static_cast<std::uint8_t>(shade),
     static_cast<std::uint8_t>(std::min(240u,shade+12u)),
     static_cast<std::uint8_t>(std::min(250u,shade+26u)),255};
   wenuygfx::addBox(scene,{at+length*.5f,y,height*.5f},
                     {length,.32f,height},color,wenuygfx::Material::Flat);
   ++displayed;
  }
 }
 // The image is a MIDI note map, not a spectrogram or a rendered audio file.
 return scene;
}
} // namespace

int main(int argc,char** argv) {
 if(argc<3||argc>4||(argc==4&&std::strcmp(argv[3],"--low")!=0)) {
  std::fprintf(stderr,"%s\n",tr("Usage: wenuy-studio-gfx PROJECT.wstudio IMAGE.ppm [--low]",
                                 "Användning: wenuy-studio-gfx PROJEKT.wstudio BILD.ppm [--low]"));
  return 2;
 }
 try {
  namespace fs=std::filesystem;
  if(fs::exists(fs::u8path(argv[2])))
   throw std::runtime_error(tr("Output image already exists","Bildfilen finns redan"));
  const auto project=wenuystudio::loadProject(argv[1]);
  const char* perf=std::getenv("WENUYOS_PERF");
  const bool low=(argc==4)||(perf&&std::strcmp(perf,"low")==0);
  const int width=low?320:640,height=low?180:360;
  const auto scene=noteScene(project);
  wenuygfx::SoftwareRenderer renderer;
  wenuygfx::Camera camera;
  camera.eye={13,-16,12};camera.target={0,0,1.4f};
  camera.fovYDegrees=58.f;camera.farPlane=70;
  wenuygfx::Frame frame;
  if(!renderer.render(scene,camera,width,height,frame) || !frame.savePPM(argv[2]))
   throw std::runtime_error(tr("Could not render or save the 3D image",
                               "Kunde inte rita eller spara 3D-bilden"));
  std::printf("%s: %s (%d x %d)\n",
    tr("3D note map saved","3D-notkarta sparad"),argv[2],width,height);
  return 0;
 }catch(const std::exception& e){
  std::fprintf(stderr,"%s: %s\n",tr("Cannot create 3D note map",
                                     "Kan inte skapa 3D-notkarta"),e.what());
  return 1;
 }
}
