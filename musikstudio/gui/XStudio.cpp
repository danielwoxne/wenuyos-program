// SPDX-License-Identifier: GPL-3.0-or-later

// Lightweight X11/XWayland presentation layer for WenuyOS Studio 0.7.

// PortAudio and file operations are performed outside the X drawing functions.

#include <X11/Xlib.h>

#include <X11/Xutil.h>

#include <X11/keysym.h>

#include "wenuystudio/Engine.hpp"

#include "wenuystudio/EditModel.hpp"

#include "wenuystudio/ProjectIO.hpp"

#include "wenuystudio/Playback.hpp"

#include "wenuystudio/Wave.hpp"

#include "wenuystudio/Sequencer.hpp"

#include "wenuystudio/MidiFile.hpp"

#include "wenuystudio/Arrangement.hpp"

#include <algorithm>

#include <array>

#include <chrono>

#include <cmath>

#include <cstring>

#include <filesystem>

#include <iostream>

#include <unordered_map>

#include <cstdlib>

#include <stdexcept>

#include <string>

#include <thread>

#include <utility>

#include <vector>

#include <sys/select.h>

namespace ws=wenuystudio;

namespace fs=std::filesystem;

namespace {

constexpr unsigned long BACK=0x101827,PANEL=0x1B2536,PANEL2=0x25344B,TEXT=0xE4EAF4,MUTED=0xA3B4C9,

 ACCENT=0x409DFF,BORDER=0x3A4A60,GOOD=0x26CC9B,WARN=0xFFBF69,DANGER=0xFF697E;

constexpr std::array<unsigned long,8> COLORS={0x48A7FF,0xDD8AFF,0x44D7B6,0xF9A764,0xCF86F1,0x70C1DD,0x9FB7EF,0xFFC77F};



static bool localeSwedish(){

 const char* lang=std::getenv("LANGUAGE");

 if(!lang||!*lang)lang=std::getenv("LC_ALL");

 if(!lang||!*lang)lang=std::getenv("LANG");

 return lang && std::string(lang).rfind("sv",0)==0;

}

static std::string translate(const std::string& english,bool swedish){

 if(!swedish)return english;

 static const std::unordered_map<std::string,std::string> words={

  {"New","Nytt"},{"Open","Öppna"},{"Save","Spara"},{"WAV in","Importera WAV"},

  {"WAV out","Exportera WAV"},{"MIDI in","Importera MIDI"},{"MIDI out","Exportera MIDI"},

  {"Play","Spela"},{"Stop","Stoppa"},{"Record","Spela in"},{"+ Track","+ Spår"},

  {"Loop","Loop"},{"Arrangement","Arrangemang"},{"Piano roll","Pianorulle"},

  {"Mixer + FX","Mixer + effekter"},{"Undo","Ångra"},{"Redo","Gör om"},

  {"Language","Språk"},{"Untitled session","Namnlöst projekt"},{"Session saved","Projekt sparat"},

  {"Unsaved changes","Osparade ändringar"},{"TRACKS","SPÅR"},

  {"Click to edit","Klicka för att redigera"},{"FX enabled","Effekter på"},

  {"FX bypass","Effekter av"},{"Mute: off","Tyst: av"},{"MUTE ON","TYST: PÅ"},

  {"Solo: off","Solo: av"},{"SOLO ON","SOLO: PÅ"},

  {"FX on/off","Effekt på/av"},

  {"Open .wstudio project","Öppna .wstudio-projekt"},

  {"Save .wstudio project","Spara .wstudio-projekt"},

  {"Import WAV audio file","Importera WAV-ljudfil"},

  {"Export mixed audio WAV","Exportera mixad WAV-fil"},

  {"Import standard MIDI file","Importera standard-MIDI"},

  {"Export standard MIDI file","Exportera standard-MIDI"},

  {"Record microphone to a new WAV file","Spela in mikrofon till ny WAV-fil"},

  {"Full file path (type or paste):","Fullständig sökväg (skriv eller klistra in):"},

  {"No X11/XWayland display. WenuyOS needs XWayland for this build.",

     "Ingen X11/XWayland-skärm. Denna version behöver XWayland."},

  {"Nothing to undo","Inget att ångra"},{"Nothing to redo","Inget att göra om"},

  {"Undo successful","Ändringen ångrades"},{"Redo successful","Ändringen gjordes om"},

  {"Session changed","Projektet ändrades"},

  {"Recording microphone... press Record to stop","Spelar in mikrofon ... tryck Spela in för att stoppa"},

  {"Ready - Ctrl+S save - Space play/pause","Redo - Ctrl+S sparar - blanksteg spelar/pausar"}

 };

 const auto it=words.find(english);return it==words.end()?english:it->second;

}



enum class Page{Arrangement,Piano,Mixer};

enum class Prompt{Idle,Open,Save,ImportWav,ExportWav,ImportMidi,ExportMidi,RecordWav};

struct Rect{int x=0,y=0,w=0,h=0;bool contains(int a,int b)const{return a>=x&&b>=y&&a<x+w&&b<y+h;}};

struct Button{Rect r;std::string label;int code;};

struct App{

 Display* d=nullptr;Window win=0;GC gc=nullptr;XFontStruct* font=nullptr;XFontSet fontSet=nullptr;bool swedish=localeSwedish();

 int width=1280,height=800;bool running=true,dirty=false,smoke=false;

 ws::Project project;ws::Playback playback;ws::Recording recording;ws::EditHistory edits;

 std::string projectPath,recordFile;double recordAt=0;

 std::string message="Ready - Ctrl+S save - Space play/pause";

 Page page=Page::Arrangement;Prompt prompt=Prompt::Idle;

 std::string input;std::vector<Button> buttons;

 unsigned selectedTrack=0;int selectedNote=-1;double playhead=0;double pixelsPerSecond=75;

 int trackOffset=0,dragTrack=-1,dragClip=-1;int dragStart=0;

 int selectedFader=-1;bool dragSaved=false;

 std::chrono::steady_clock::time_point lastSave=std::chrono::steady_clock::now();

 App(){project.sampleRate=48000;project.durationSeconds=16;project.bpm=110;

  for(const char* n:{"Lead synth","Harmony","Bass","Audio"}){ws::Track t;t.name=n;project.tracks.push_back(std::move(t));}

  const int melody[]={60,64,67,72,71,69,67,64};

  for(int i=0;i<8;++i)project.tracks[0].notes.push_back({melody[i],i*.54,.45,.75f});

  for(int i=0;i<4;++i)project.tracks[2].notes.push_back({i%2?43:36,i*2.18,1.8,.6f});

  project.tracks[0].gain=.7f;project.tracks[2].gain=.6f;

 }

 ~App(){playback.stop();recording.stop();if(fontSet)XFreeFontSet(d,fontSet);if(font)XFreeFont(d,font);if(gc)XFreeGC(d,gc);if(d)XCloseDisplay(d);}

 void init(){

  d=XOpenDisplay(nullptr);if(!d)throw std::runtime_error("No X11/XWayland display. WenuyOS needs XWayland for this build.");

  int sc=DefaultScreen(d);win=XCreateSimpleWindow(d,RootWindow(d,sc),50,50,width,height,0,0,BACK);

  XStoreName(d,win,"Wenuy Studio 0.8 - Musikstudio");

  XSelectInput(d,win,ExposureMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|KeyPressMask|StructureNotifyMask);

  Atom close=XInternAtom(d,"WM_DELETE_WINDOW",False);XSetWMProtocols(d,win,&close,1);

  gc=XCreateGC(d,win,0,nullptr);

  font=XLoadQueryFont(d,"10x20");

  if(!font)font=XLoadQueryFont(d,"9x15");

  if(!font)font=XLoadQueryFont(d,"fixed");

  if(font)XSetFont(d,gc,font->fid);

  char** missing=nullptr;int numMissing=0;char* defaultChar=nullptr;

  fontSet=XCreateFontSet(d,"-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1",&missing,&numMissing,&defaultChar);

  if(missing)XFreeStringList(missing);

  XMapWindow(d,win);XFlush(d);

 }

 void color(unsigned long c){XSetForeground(d,gc,c);}

 void fill(int x,int y,int w,int h,unsigned long c){if(w<=0||h<=0)return;color(c);XFillRectangle(d,win,gc,x,y,unsigned(w),unsigned(h));}

 void line(int x,int y,int xx,int yy,unsigned long c){color(c);XDrawLine(d,win,gc,x,y,xx,yy);}

 void txt(int x,int y,std::string s,unsigned long c=TEXT){if(s.empty())return;color(c);s=translate(s,swedish);if(s.size()>160)s.resize(160);

  if(fontSet)Xutf8DrawString(d,win,fontSet,gc,x,y,s.c_str(),int(s.size()));

  else XDrawString(d,win,gc,x,y,s.c_str(),int(s.size()));}

 void frame(Rect r,unsigned long c=BORDER){color(c);XDrawRectangle(d,win,gc,r.x,r.y,unsigned(std::max(1,r.w-1)),unsigned(std::max(1,r.h-1)));}

 void button(int x,int y,int w,int h,std::string label,int code,bool active=false){

  Rect r{x,y,w,h};fill(x,y,w,h,active?ACCENT:PANEL2);frame(r,active?ACCENT:BORDER);

  txt(x+10,y+h/2+5,label,active?BACK:TEXT);buttons.push_back({r,label,code});

 }

 void render(){

  buttons.clear();fill(0,0,width,height,BACK);

  fill(0,0,width,68,PANEL);

  txt(24,27,"WENUY",ACCENT);txt(84,27,"STUDIO",TEXT);txt(24,48,"MUSIKSTUDIO  0.8.0",MUTED);

  std::string name=projectPath.empty()?"Untitled session":fs::path(projectPath).filename().string();

  if(name.size()>50)name.resize(50);

  txt(270,27,name,TEXT);txt(270,48,dirty?"Unsaved changes":"Session saved",dirty?WARN:GOOD);

  txt(width-267,28,"TEMPO  "+std::to_string(int(project.bpm))+" BPM",TEXT);

  button(width-133,35,40,24,"-",30);button(width-85,35,40,24,"+",31);

  int x=16,y=78;

  const std::vector<std::pair<const char*,int>> tools={{"New",1},{"Open",2},{"Save",3},{"WAV in",4},{"WAV out",5},

     {"MIDI in",6},{"MIDI out",7},{"Play",8},{"Stop",9},{"Record",12},{"+ Track",10},{"Loop",11}};

  int wt=std::max(54,std::min(97,(width-35)/int(tools.size())-5));

  for(auto& t:tools){button(x,y,wt,40,t.first,t.second,(t.second==8&&playback.active())||(t.second==11&&project.loop.enabled)||(t.second==12&&recording.active()));x+=wt+5;}

  fill(0,128,width,54,BACK);

  button(16,135,140,36,"Arrangement",20,page==Page::Arrangement);

  button(164,135,140,36,"Piano roll",21,page==Page::Piano);

  button(312,135,120,36,"Mixer + FX",22,page==Page::Mixer);

  button(445,135,90,36,"Undo",40);button(540,135,90,36,"Redo",41);

  if(width>900)button(width-150,135,130,36,"Language",42);

  txt(width-250,159,"Tracks: "+std::to_string(project.tracks.size())+"   48 kHz",MUTED);

  if(page==Page::Arrangement)drawArrangement();

  if(page==Page::Piano)drawPiano();

  if(page==Page::Mixer)drawMixer();

  fill(0,height-53,width,53,PANEL);

  if(!playback.error().empty())message="Sound: "+playback.error();

  if(recording.active())message="Recording microphone... press Record to stop";

  txt(22,height-31,message.size()>120?message.substr(0,120):message,MUTED);

  txt(22,height-10,"Ctrl+S Save   Ctrl+Z Undo   Ctrl+Y Redo   Space Play   Q Quantize   A Add note   N New track   Delete Remove note",MUTED);

  if(prompt!=Prompt::Idle)drawPrompt();

  XFlush(d);

 }

 void ruler(int left,int top,int right){

  const int w=right-left;fill(left,top,w,33,PANEL2);

  const int step=int(std::max(1.,std::round(100./pixelsPerSecond)));

  for(int sec=0;sec<=int(project.durationSeconds);sec+=step){int xx=left+int(sec*pixelsPerSecond);

   if(xx>=right)break;line(xx,top+19,xx,top+33,BORDER);txt(xx+4,top+14,std::to_string(sec)+"s",MUTED);}

 }

 void drawArrangement(){

  int top=197,left=248,right=width-18;

  fill(14,top,220,height-top-73,PANEL);

  fill(248,top,right-left,height-top-73,PANEL);

  ruler(left,top,right);

  txt(28,top+21,"TRACKS",MUTED);

  int usable=(height-73-(top+38))/86;

  if(trackOffset>int(project.tracks.size())-1)trackOffset=std::max(0,int(project.tracks.size())-1);

  for(int row=0;row<usable;++row){

   int t=row+trackOffset; if(t>=int(project.tracks.size()))break;

   auto& tr=project.tracks[t];int yy=top+40+row*86;

   fill(16,yy,218,81,t==int(selectedTrack)?PANEL2:PANEL);

   fill(16,yy,5,81,COLORS[t%COLORS.size()]);

   txt(30,yy+25,tr.name.size()>26?tr.name.substr(0,26):tr.name,t==int(selectedTrack)?TEXT:MUTED);

   txt(30,yy+52,"Vol "+std::to_string(int(tr.gain*100))+"%",MUTED);

   txt(131,yy+52,tr.mute?"MUTED":(tr.solo?"SOLO":"        "),tr.mute?DANGER:GOOD);

   fill(left,yy,right-left,81,row%2?0x1D2A3B:0x202E42);

   for(int sec=0;sec<=int(project.durationSeconds);sec+=2){int xx=left+int(sec*pixelsPerSecond);

    if(xx>=right)break;line(xx,yy,xx,yy+81,0x304056);}

   for(const auto& c:tr.clips){

    const auto x=left+int(c.startSeconds*pixelsPerSecond);

    const auto len=c.durationSeconds>0?c.durationSeconds:2.;

    const int rw=std::max(8,int(len*pixelsPerSecond));if(x>=right||x+rw<left)continue;

    fill(x,yy+8,std::min(rw,right-x),64,0x28628B);frame({x,yy+8,std::min(rw,right-x),64},COLORS[t%8]);

    std::string filename=fs::path(c.filename).filename().string();txt(x+5,yy+30,filename.substr(0,20));

    for(int j=8;j<rw-5;j+=8){int xx=x+j;if(xx>=right)break;line(xx,yy+48-int(j%5)*3,xx,yy+53+int(j%4)*2,ACCENT);}

   }

   for(const auto& n:tr.notes){

    int xx=left+int(n.atSeconds*pixelsPerSecond),ww=std::max(5,int(n.durationSeconds*pixelsPerSecond));

    int nY=yy+10+(77-n.key)*2;if(nY<yy+8||nY>yy+65)nY=yy+35;

    if(xx>=left&&xx<right){fill(xx,nY,std::min(ww,right-xx),8,COLORS[t%8]);}

   }

  }

  int mark=left+int((playback.active()?playback.position():playhead)*pixelsPerSecond);

  if(mark>=left&&mark<right)line(mark,top+1,mark,height-75,GOOD);

  txt(20,height-78,"Click lane to select. Right-click an audio clip to split; middle-click to duplicate.",MUTED);

 }

 void drawPiano(){

  int top=228,left=100,right=width-20,bottom=height-81;

  fill(16,190,width-32,32,PANEL);

  txt(25,211,"PIANO ROLL  -  "+project.tracks[std::min<std::size_t>(selectedTrack,project.tracks.size()-1)].name,TEXT);

  txt(width-370,211,"Click: add note     Right click: remove     Q: snap",MUTED);

  fill(14,top,85,bottom-top,PANEL);fill(left,top,right-left,bottom-top,PANEL);

  int base=36,upper=84;double rowHeight=double(bottom-top)/(upper-base);

  for(int key=upper-1;key>=base;--key){int yy=top+int((upper-1-key)*rowHeight);

   bool black=(key%12==1||key%12==3||key%12==6||key%12==8||key%12==10);

   fill(15,yy,83,std::max(1,int(rowHeight)),black?0x1A2738:0x293A50);

   fill(left,yy,right-left,std::max(1,int(rowHeight)),black?0x142234:0x1B2C42);

   line(left,yy,right,yy,0x2A3A50);

   if(key%12==0)txt(28,yy+12,"C"+std::to_string(key/12-1),TEXT);

  }

  double pxPerBeat=60;double secPerBeat=60./project.bpm;

  double pxPerSecond=pxPerBeat/secPerBeat;

  for(int beat=0;beat<160;++beat){int xx=left+int(beat*pxPerBeat);

   if(xx>=right)break;line(xx,top,xx,bottom,beat%4==0?BORDER:0x263954);

   if(beat%4==0)txt(xx+4,top-8,"BAR "+std::to_string(beat/4+1),MUTED);

  }

  const auto& notes=project.tracks[selectedTrack].notes;

  for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];

   int x=left+int(n.atSeconds*pxPerSecond),y=top+int((upper-1-n.key)*rowHeight);

   int w=std::max(5,int(n.durationSeconds*pxPerSecond));if(x>=right||x+w<left||y<top||y>=bottom)continue;

   fill(x,y,std::min(w,right-x),std::max(4,int(rowHeight)-2),i==std::size_t(selectedNote)?WARN:ACCENT);

  }

  int barx=left+int((playback.active()?playback.position():playhead)*pxPerSecond);

  if(barx>=left&&barx<right)line(barx,top,barx,bottom,GOOD);

 }

 void drawMixer(){

  int top=207,left=20,step=150,maxCount=std::max(1,(width-40)/step);

  for(int i=0;i<maxCount&&i<int(project.tracks.size());++i){

   const auto& t=project.tracks[i];int x=left+i*step;int h=height-290;

   fill(x,top,step-9,h,PANEL);frame({x,top,step-9,h},i==int(selectedTrack)?ACCENT:BORDER);

   fill(x+8,top+10,step-25,5,COLORS[i%8]);

   txt(x+14,top+40,t.name.substr(0,14),TEXT);

   const char* instruments[]={"Sine","Triangle","Organ","Warm lead"};

   txt(x+14,top+58,std::string("INST: ")+instruments[std::clamp(t.instrument,0,3)],ACCENT);

   txt(x+14,top+72,t.mute?"MUTE ON":"Mute: off",t.mute?DANGER:MUTED);

   txt(x+14,top+97,t.solo?"SOLO ON":"Solo: off",t.solo?GOOD:MUTED);

   txt(x+14,top+130,"VOL "+std::to_string(int(t.gain*100))+"%",TEXT);

   fill(x+61,top+145,18,h-245,0x36485D);

   int fh=h-245;int y=top+145+fh-int(fh*std::min(2.f,t.gain)/2.f);

   fill(x+61,y,18,top+145+fh-y,COLORS[i%8]);fill(x+52,y-4,36,8,ACCENT);

   txt(x+14,top+h-78,"PAN "+std::to_string(int(t.pan*100)),MUTED);

   txt(x+14,top+h-51,t.effects.enabled?"FX enabled":"FX bypass",t.effects.enabled?GOOD:MUTED);

   txt(x+14,top+h-26,"Click to edit",ACCENT);

  }

  int y=height-142;fill(16,y,width-32,58,PANEL2);

  const auto& e=project.tracks[selectedTrack].effects;

  txt(28,y+23,"EQ / COMP "+project.tracks[selectedTrack].name,TEXT);

  txt(28,y+45,"Click values to adjust. FX = "+std::string(e.enabled?"ON":"OFF"),e.enabled?GOOD:MUTED);

  button(305,y+10,100,38,"Low "+std::to_string(int(e.lowDb)),50);

  button(414,y+10,100,38,"Mid "+std::to_string(int(e.midDb)),51);

  button(523,y+10,100,38,"High "+std::to_string(int(e.highDb)),52);

  button(633,y+10,125,38,"Thresh "+std::to_string(int(e.thresholdDb)),53);

  button(766,y+10,95,38,"Ratio "+std::to_string(int(e.ratio)),54);

  button(870,y+10,93,38,"FX on/off",55,e.enabled);

  txt(width-260,height-90,"Master "+std::to_string(int(project.masterGain*100))+"%",MUTED);

 }

 void drawPrompt(){

  int w=std::min(710,width-80),h=170,x=(width-w)/2,y=(height-h)/2;

  fill(x-5,y-5,w+10,h+10,ACCENT);fill(x,y,w,h,PANEL);

  std::string title;

  switch(prompt){case Prompt::Open:title="Open .wstudio project";break;

   case Prompt::Save:title="Save .wstudio project";break;

   case Prompt::ImportWav:title="Import WAV audio file";break;

   case Prompt::ExportWav:title="Export mixed audio WAV";break;

   case Prompt::ImportMidi:title="Import standard MIDI file";break;

   case Prompt::ExportMidi:title="Export standard MIDI file";break;

   case Prompt::RecordWav:title="Record microphone to a new WAV file";break;

   default:break;}

  txt(x+20,y+32,title);txt(x+20,y+65,"Full file path (type or paste):",MUTED);

  fill(x+20,y+78,w-40,35,PANEL2);frame({x+20,y+78,w-40,35},BORDER);

  txt(x+28,y+100,input.size()>84?input.substr(input.size()-84):input,ACCENT);

  txt(x+20,y+146,"ENTER: Confirm       ESC: Cancel       Files are not overwritten by exports",MUTED);

 }

 void changed(){dirty=true;message="Session changed";}

 void remember(){edits.checkpoint(project);}

 void restore(bool redo){

  playback.stop();

  if(redo?edits.redo(project):edits.undo(project)){

   selectedTrack=unsigned(std::min<std::size_t>(selectedTrack,project.tracks.empty()?0:project.tracks.size()-1));

   selectedNote=-1;playhead=std::min(playhead,project.durationSeconds);changed();

   message=redo?"Redo successful":"Undo successful";

  }else message=redo?"Nothing to redo":"Nothing to undo";

 }

 void save(){if(projectPath.empty()){prompt=Prompt::Save;input="session.wstudio";return;}

  try{ws::saveProject(project,projectPath);dirty=false;message="Saved "+projectPath;lastSave=std::chrono::steady_clock::now();}

  catch(const std::exception& e){message=e.what();}

 }

 void startPrompt(Prompt which){prompt=which;input="";

  if(which==Prompt::Save)input=projectPath.empty()?"session.wstudio":projectPath;

  if(which==Prompt::ExportWav)input="mix.wav";

  if(which==Prompt::ExportMidi)input="melody.mid";

  if(which==Prompt::RecordWav)input="recording.wav";

 }

 void commitPrompt(){auto which=prompt;prompt=Prompt::Idle;auto path=input;input.clear();if(path.empty())return;

  try{

   switch(which){

    case Prompt::Open:playback.stop();recording.stop();finalizeRecord();project=ws::loadProject(path);projectPath=path;if(project.tracks.empty()){ws::Track tr;tr.name="Track 1";project.tracks.push_back(tr);}

      selectedTrack=0;selectedNote=-1;playhead=0;dirty=false;edits.clear();message="Opened "+path;break;

    case Prompt::Save:projectPath=path;save();break;

    case Prompt::ImportWav:{

     ws::WaveReader wav(path);

     if(project.tracks.size()>=128)throw std::runtime_error("Maximum 128 tracks");

     ws::Track t;t.name=fs::path(path).stem().string();

     const double duration=double(wav.frames())/wav.rate();

     t.clips.push_back({fs::absolute(path).string(),playhead,0,duration,1});

     project.durationSeconds=std::min(20000.,std::max(project.durationSeconds,playhead+duration));

     remember();project.tracks.push_back(std::move(t));selectedTrack=unsigned(project.tracks.size()-1);changed();message="Imported WAV";break;}

    case Prompt::ExportWav:ws::render(project,path);message="Rendered WAV: "+path;break;

    case Prompt::ImportMidi:{auto midi=ws::importMidi(path);if(midi.tracks.size()+project.tracks.size()>128)throw std::runtime_error("Too many imported tracks");

      project.durationSeconds=std::min(20000.,std::max(project.durationSeconds,midi.durationSeconds));

      remember();for(auto& track:midi.tracks)project.tracks.push_back(std::move(track));changed();message="Imported MIDI";break;}

    case Prompt::ExportMidi:if(fs::exists(path))throw std::runtime_error("MIDI export already exists");ws::exportMidi(project,path);message="Exported MIDI";break;

    case Prompt::RecordWav:playback.stop();recordAt=playhead;recordFile=path;recording.start(path,project.sampleRate);message="Recording... press Record to stop";break;

    default:break;

   }

  }catch(const std::exception& e){message=std::string("Error: ")+e.what();}

 }

 void finalizeRecord(){

  if(recordFile.empty())return;

  recording.stop();

  if(recording.completed()&&fs::exists(recordFile)){

   try{ws::WaveReader reader(recordFile);

    double len=double(reader.frames())/reader.rate();

    if(project.tracks.size()<128){ws::Track tr;tr.name="Microphone take";

     tr.clips.push_back({fs::absolute(recordFile).string(),recordAt,0,len,1});

     remember();project.tracks.push_back(std::move(tr));selectedTrack=unsigned(project.tracks.size()-1);

     project.durationSeconds=std::min(20000.,std::max(project.durationSeconds,recordAt+len));changed();

     message="Recording saved to "+recordFile;

    }

   }catch(const std::exception& e){message=e.what();}

  }else{message="Recording failed: "+recording.error();}

  recordFile.clear();

 }

 void activate(int code,int x=0){

  try{

   switch(code){

    case 1:playback.stop();recording.stop();finalizeRecord();edits.clear();project=ws::Project{};project.tracks.push_back(ws::Track{});project.tracks[0].name="Track 1";projectPath.clear();playhead=0;selectedTrack=0;selectedNote=-1;changed();break;

    case 2:startPrompt(Prompt::Open);break;case 3:save();break;

    case 4:startPrompt(Prompt::ImportWav);break;case 5:startPrompt(Prompt::ExportWav);break;

    case 6:startPrompt(Prompt::ImportMidi);break;case 7:startPrompt(Prompt::ExportMidi);break;

    case 8:playback.start(project,std::min(playhead,std::max(0.,project.durationSeconds-.02)));message="Playback started";break;

    case 9:playhead=std::min(playback.position(),project.durationSeconds);playback.stop();message="Stopped";break;

    case 12:if(recording.active()){recording.stop();finalizeRecord();}

      else startPrompt(Prompt::RecordWav);break;

    case 10:if(project.tracks.size()<128){remember();ws::Track t;t.name="Track "+std::to_string(project.tracks.size()+1);project.tracks.push_back(std::move(t));selectedTrack=unsigned(project.tracks.size()-1);changed();}break;

    case 11:{remember();auto old=project.loop.enabled;project.loop={!old,0,std::min(project.durationSeconds,4*60/project.bpm*4)};changed();break;}

    case 40:restore(false);break;case 41:restore(true);break;case 42:swedish=!swedish;break;

    case 20:page=Page::Arrangement;break;case 21:page=Page::Piano;break;case 22:page=Page::Mixer;break;

    case 30:remember();project.bpm=std::max(20.,project.bpm-1);changed();break;

    case 31:remember();project.bpm=std::min(400.,project.bpm+1);changed();break;

    case 50:case 51:case 52:{remember();auto& fx=project.tracks[selectedTrack].effects;float* f=code==50?&fx.lowDb:code==51?&fx.midDb:&fx.highDb;

      *f=std::clamp(*f+(x<0?-1.f:1.f),-18.f,18.f);fx.enabled=true;changed();break;}

    case 53:{remember();auto& fx=project.tracks[selectedTrack].effects;fx.thresholdDb=std::clamp(fx.thresholdDb+float(x<0?-1:1),-48.f,0.f);fx.enabled=true;changed();break;}

    case 54:{remember();auto& fx=project.tracks[selectedTrack].effects;fx.ratio=std::clamp(fx.ratio+float(x<0?-1:1),1.f,12.f);fx.enabled=true;changed();break;}

    case 55:{remember();auto& fx=project.tracks[selectedTrack].effects;fx.enabled=!fx.enabled;changed();break;}

    default:break;

   }

  }catch(const std::exception& e){message=std::string("Error: ")+e.what();}

 }

 void click(int x,int y,unsigned btn){

  if(prompt!=Prompt::Idle)return;

  for(auto& b:buttons)if(b.r.contains(x,y)){activate(b.code,(x<b.r.x+b.r.w/2?-1:1));return;}

  if(page==Page::Arrangement){

   int top=237;int row=(y-top)/86;int tr=row+trackOffset;

   if(y<top||tr<0||tr>=int(project.tracks.size()))return;

   selectedTrack=unsigned(tr);selectedNote=-1;

   if(x<248){if(x>130&&y>=top+row*86+36){remember();project.tracks[tr].mute=!project.tracks[tr].mute;changed();}return;}

   const double sec=std::clamp(double(x-248)/pixelsPerSecond,0.,project.durationSeconds);

   playhead=sec;

   const auto& clips=project.tracks[tr].clips;

   for(std::size_t i=0;i<clips.size();++i){const auto& c=clips[i];

    if(sec>=c.startSeconds&&sec<=c.startSeconds+(c.durationSeconds>0?c.durationSeconds:2.)){

     if(btn==3){remember();if(ws::splitAudioClip(project,tr,i,sec)){changed();message="Audio clip split";}}

     else if(btn==2){remember();if(ws::duplicateAudioClip(project,tr,i,sec+1)){changed();message="Audio clip duplicated";}}

     else {dragTrack=tr;dragClip=int(i);dragStart=x;dragSaved=false;}

     break;

    }

   }

  }else if(page==Page::Piano){

   int top=228,left=100,bottom=height-81;if(x<left||y<top||y>=bottom)return;

   int upper=84,base=36;double rowHeight=double(bottom-top)/(upper-base);

   int key=std::clamp(upper-1-int((y-top)/rowHeight),base,upper-1);

   double sec=std::max(0.,double(x-left)/(60./(60./project.bpm)));

   auto& notes=project.tracks[selectedTrack].notes;

   for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];if(n.key==key&&sec>=n.atSeconds&&sec<n.atSeconds+n.durationSeconds){

     if(btn==3){remember();ws::removeNote(project,selectedTrack,i);selectedNote=-1;changed();}

     else selectedNote=int(i);return;}}

   if(btn==1){remember();auto result=ws::insertNote(project,selectedTrack,{key,sec,60./project.bpm*.75,.8f},{4,0});

     if(result.changed){selectedNote=int(result.index);changed();}}

  }else if(page==Page::Mixer){

   int top=207,step=150,i=(x-20)/step;if(i<0||i>=int(project.tracks.size())||i>=(width-40)/step)return;

   selectedTrack=unsigned(i);auto& t=project.tracks[i];int localX=(x-20)%step;int localY=y-top;

   if(localY>35&&localY<65){remember();t.instrument=(t.instrument+1)%4;changed();}

   else if(localY>65&&localY<82){remember();t.mute=!t.mute;changed();}

   else if(localY>=82&&localY<112){remember();t.solo=!t.solo;changed();}

   else if(localY>140&&localY<height-290-99&&localX>38&&localX<100){

     double ratio=1.-double(localY-145)/std::max(1,height-290-245);

     remember();t.gain=std::clamp(float(ratio*2),0.f,2.f);changed();}

   else if(localY>height-290-100&&localY<height-290-67){remember();t.pan=std::clamp(float((localX-75)/70.),-1.f,1.f);changed();}

  }

 }

 void onKey(XKeyEvent& ev){

  KeySym sym=0;char b[20]{};int n=XLookupString(&ev,b,sizeof(b),&sym,nullptr);

  if(prompt!=Prompt::Idle){

   if(sym==XK_Escape){prompt=Prompt::Idle;input.clear();}

   else if(sym==XK_Return||sym==XK_KP_Enter)commitPrompt();

   else if(sym==XK_BackSpace){if(!input.empty())input.pop_back();}

   else if(n>0&&input.size()<1024&&b[0]>=32&&b[0]<127)input.append(b,unsigned(n));

   return;

  }

  bool ctrl=(ev.state&ControlMask)!=0;

  if(ctrl&&(sym==XK_s||sym==XK_S)){save();return;}

  if(ctrl&&(sym==XK_o||sym==XK_O)){startPrompt(Prompt::Open);return;}

  if(ctrl&&(sym==XK_z||sym==XK_Z)){restore((ev.state&ShiftMask)!=0);return;}

  if(ctrl&&(sym==XK_y||sym==XK_Y)){restore(true);return;}

  if(sym==XK_space){if(playback.active())activate(9);else activate(8);return;}

  if(sym==XK_Escape){activate(9);return;}

  if(sym==XK_n||sym==XK_N){activate(10);return;}

  if(sym==XK_q||sym==XK_Q){

   try{remember();ws::quantizeTrack(project,selectedTrack,{4,0});changed();message="Quantized selected track";}

   catch(const std::exception& e){message=e.what();}return;

  }

  if(sym==XK_a||sym==XK_A){

   try{remember();ws::insertNote(project,selectedTrack,{60,playhead,60/project.bpm*.8,.8f},{4,0});changed();}

   catch(const std::exception& e){message=e.what();}return;

  }

  if((sym==XK_Delete||sym==XK_BackSpace)&&selectedNote>=0){remember();ws::removeNote(project,selectedTrack,unsigned(selectedNote));selectedNote=-1;changed();return;}

  if(sym==XK_Up){if(selectedTrack>0)--selectedTrack;}

  if(sym==XK_Down){if(selectedTrack+1<project.tracks.size())++selectedTrack;}

  if(sym==XK_Right)playhead=std::min(project.durationSeconds,playhead+60./project.bpm/4);

  if(sym==XK_Left)playhead=std::max(0.,playhead-60./project.bpm/4);

 }

 void loop(){

  init();if(smoke){XSync(d,False);std::cout<<"GUI smoke PASS: X11 window mapped\n";return;}

  bool repaint=true;auto last=std::chrono::steady_clock::now();

  while(running){

   while(XPending(d)){

    XEvent ev{};XNextEvent(d,&ev);

    if(ev.type==Expose){repaint=true;}

    else if(ev.type==ConfigureNotify){width=std::max(750,ev.xconfigure.width);height=std::max(600,ev.xconfigure.height);repaint=true;}

    else if(ev.type==ClientMessage){running=false;}

    else if(ev.type==KeyPress){onKey(ev.xkey);repaint=true;}

    else if(ev.type==ButtonPress){

     if(ev.xbutton.button==4&&page==Page::Arrangement){trackOffset=std::max(0,trackOffset-1);}

     else if(ev.xbutton.button==5&&page==Page::Arrangement){trackOffset=std::min(int(project.tracks.size())-1,trackOffset+1);}

     else click(ev.xbutton.x,ev.xbutton.y,ev.xbutton.button);

     repaint=true;

    }else if(ev.type==ButtonRelease){dragTrack=dragClip=-1;dragSaved=false;}

    else if(ev.type==MotionNotify&&dragTrack>=0&&dragClip>=0){

     auto sec=std::max(0.,double(ev.xmotion.x-248)/pixelsPerSecond);

     sec=ws::snapTime(sec,project.bpm,{4,0});

     if(!dragSaved){remember();dragSaved=true;}

     if(ws::moveAudioClip(project,unsigned(dragTrack),unsigned(dragClip),sec)){changed();repaint=true;}

    }

   }

   auto now=std::chrono::steady_clock::now();

   if(playback.active()&&now-last>std::chrono::milliseconds(90))repaint=true;

   if(!recordFile.empty()&&!recording.active()){finalizeRecord();repaint=true;}

   if(recording.active()&&now-last>std::chrono::milliseconds(100))repaint=true;

   if(now-last>std::chrono::milliseconds(900))repaint=true;

   if(dirty&&!projectPath.empty()&&now-lastSave>std::chrono::seconds(45)){

    try{ws::saveProject(project,projectPath+".recovery");lastSave=now;message="Recovery checkpoint ready";}

    catch(const std::exception& e){message=std::string("Autosave: ")+e.what();lastSave=now;}

    repaint=true;

   }

   if(repaint){render();last=now;repaint=false;}

   fd_set r;FD_ZERO(&r);FD_SET(ConnectionNumber(d),&r);timeval timeout{0,45000};

   select(ConnectionNumber(d)+1,&r,nullptr,nullptr,&timeout);

  }

 }

};

}

int main(int argc,char** argv){

 try{

  App app;for(int i=1;i<argc;++i){if(std::strcmp(argv[i],"--smoke-test")==0)app.smoke=true;

   else if(argv[i][0]!='-'){app.project=ws::loadProject(argv[i]);app.projectPath=argv[i];}}

  app.loop();return 0;

 }catch(const std::exception& e){std::cerr<<"Wenuy Studio error: "<<e.what()<<'\n';return 1;}

}
