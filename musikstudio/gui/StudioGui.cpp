// SPDX-License-Identifier: GPL-3.0-or-later

// Native Wenuy Studio GTK 4 interface. libwenuy is optional at compile time.

#include <adwaita.h>

#include <algorithm>

#include <cmath>

#include <cstdio>

#include <cstring>

#include <filesystem>

#include <memory>

#include <stdexcept>

#include <string>

#include <vector>

#include <unordered_map>

#include "StudioWindow.hpp"

#include "wenuystudio/Engine.hpp"

#include "wenuystudio/ProjectIO.hpp"

#include "wenuystudio/AudioIO.hpp"

#include "wenuystudio/History.hpp"

#include "wenuystudio/Waveform.hpp"

#include "wenuystudio/Recovery.hpp"

#include "wenuystudio/MediaCheck.hpp"

#ifdef HAVE_WENUY

#include <WenuyTimeline.h>

#endif

namespace fs=std::filesystem;

namespace {

constexpr char kAppId[]="se.wenuyos.app.musikstudio";

constexpr double kMaxLength=20000.0;

bool gSmokeSucceeded=false;

struct Studio {

 GtkWidget* window=nullptr;

 GtkWidget* tracksBox=nullptr;

 GtkWidget* timelineWidget=nullptr;

 GtkWidget* renderButton=nullptr;

 GtkWidget* tempoLabel=nullptr;

 GtkWidget* titleLabel=nullptr;

 GtkWidget* recordButton=nullptr;

 GtkWidget* meterLabel=nullptr;

 GtkWidget* masterScale=nullptr;

 GtkWidget* bufferDropdown=nullptr;

 wenuystudio::BufferPreset bufferPreset=wenuystudio::BufferPreset::Balanced;

 bool updatingControls=false;

 wenuystudio::Playback playback;

 wenuystudio::Recording recording;

 wenuystudio::History history;

 std::string recordTarget;

 double recordAt=0;

 bool wasPlaying=false,wasRecording=false;

 guint tickSource=0,autosaveSource=0;

 std::uint64_t changeVersion=0,autosavedVersion=0;

 bool autosaveBlocked=false,autosaveErrorShown=false;

 wenuystudio::Project project;

 std::string projectFile;

 bool dirty=false, exporting=false;

 double zoom=58.0,playhead=0.0;

 std::unordered_map<std::string,double> durationCache;

 std::unordered_map<std::string,std::vector<wenuystudio::Peak>> peakCache;

 ~Studio(){if(tickSource)g_source_remove(tickSource);if(autosaveSource)g_source_remove(autosaveSource);playback.stop();recording.stop();}

#ifdef HAVE_WENUY

 std::unordered_map<int,float> gainByClipId;

#endif

#ifdef HAVE_WENUY

 wenuy::Timeline timeline;

#endif

};

struct TrackControl {Studio* studio;std::size_t index;};

struct ExportRequest {wenuystudio::Project project;std::string output;};

enum class Pick {OpenProject=1, SaveProject=2, ImportAudio=3, ExportAudio=4, RecordAudio=5};

static const char* tr(const char* en,const char* sv){return app::tr(en,sv);}

static void status(Studio* s){

 const std::string left=std::to_string(s->project.tracks.size())+tr(" tracks · "," spår · ")+

  std::to_string(static_cast<int>(s->project.sampleRate))+" Hz · "+std::to_string(static_cast<int>(s->project.bpm))+" BPM";

 std::string right=s->recording.active()?tr("● Recording…","● Spelar in…"):

  s->playback.active()?tr("▶ Playing…","▶ Spelar…"):

  s->exporting?tr("Export in progress…","Exporterar…"):

  (s->dirty?(s->changeVersion!=0&&s->autosavedVersion==s->changeVersion

    ?tr("● Unsaved (recovery copy ready)","● Osparat (återställningskopia finns)")

    :tr("● Unsaved changes","● Osparade ändringar")):tr("Saved","Sparat"));

 app::windowSetStatus(s->window,left.c_str(),right.c_str());

 if(s->titleLabel){

  const std::string name=s->projectFile.empty()?tr("Untitled project","Namnlöst projekt"):fs::path(s->projectFile).filename().string();

  gtk_label_set_text(GTK_LABEL(s->titleLabel),name.c_str());

 }

}

static void markChanged(Studio* s){s->dirty=true;++s->changeVersion;status(s);}

static void checkpoint(Studio* s){s->history.checkpoint(s->project);}

static void notify(Studio* s,const std::string& message){app::windowToast(s->window,message.c_str());}

static void error(Studio* s,const std::exception& e){notify(s,e.what());}

static double clipDuration(Studio* s,const wenuystudio::AudioClip& c){

 if(c.durationSeconds>0)return c.durationSeconds;

 auto pos=s->durationCache.find(c.filename);

 if(pos==s->durationCache.end()){

  double secs=0;

  try{wenuystudio::WaveReader rd(c.filename);secs=static_cast<double>(rd.frames())/rd.rate();}

  catch(...){secs=1.0;}

  pos=s->durationCache.emplace(c.filename,secs).first;

 }

 return std::max(0.01,pos->second-c.sourceOffsetSeconds);

}

static void recalcDuration(Studio* s){

 double maxTime=5.0;

 for(const auto& t:s->project.tracks){

  for(const auto& n:t.notes)maxTime=std::max(maxTime,n.atSeconds+n.durationSeconds);

  for(const auto& c:t.clips){

   maxTime=std::max(maxTime,c.startSeconds+clipDuration(s,c));

  }

 }

 s->project.durationSeconds=std::min(kMaxLength,std::max(s->project.durationSeconds,maxTime+1.0));

}

static void drawFallback(GtkDrawingArea*,cairo_t* cr,int width,int height,gpointer data){

 auto* s=static_cast<Studio*>(data);

 cairo_set_source_rgb(cr,.105,.12,.155);cairo_paint(cr);

 cairo_set_line_width(cr,1.0);

 for(int sec=0;sec<=static_cast<int>(s->project.durationSeconds);++sec){

  const double x=sec*s->zoom+16;

  if(x>width)break;

  cairo_set_source_rgba(cr,.83,.86,.94,sec%4==0?.24:.08);

  cairo_move_to(cr,x,23);cairo_line_to(cr,x,height);cairo_stroke(cr);

  if(sec%4==0){cairo_set_source_rgb(cr,.72,.78,.89);cairo_move_to(cr,x+4,17);

   cairo_select_font_face(cr,"Sans",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);

   cairo_set_font_size(cr,11);cairo_show_text(cr,(std::to_string(sec)+"s").c_str());}

 }

 for(std::size_t i=0;i<s->project.tracks.size();++i){

  const double y=28+i*74;

  cairo_set_source_rgba(cr,1,1,1,i%2==0?.045:.017);cairo_rectangle(cr,0,y,width,72);cairo_fill(cr);

  cairo_set_source_rgba(cr,1,1,1,.1);cairo_move_to(cr,0,y+72);cairo_line_to(cr,width,y+72);cairo_stroke(cr);

  const auto& track=s->project.tracks[i];

  for(const auto& c:track.clips){

   const double dur=clipDuration(s,c);

   const double x=16+c.startSeconds*s->zoom,ww=std::max(2.0,dur*s->zoom);

   cairo_set_source_rgb(cr,track.mute?.28:.18,track.mute?.35:.43,track.mute?.45:.83);

   cairo_rectangle(cr,x,y+5,ww,62);cairo_fill(cr);

   cairo_set_source_rgb(cr,.95,.96,1.0);cairo_move_to(cr,x+7,y+24);

   cairo_select_font_face(cr,"Sans",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_BOLD);

   cairo_set_font_size(cr,11);

   const std::string label=fs::path(c.filename).filename().string();

   cairo_save(cr);cairo_rectangle(cr,x,y+6,ww,56);cairo_clip(cr);cairo_show_text(cr,label.c_str());cairo_restore(cr);

   const auto peaks=s->peakCache.find(c.filename);

   if(peaks!=s->peakCache.end()&&!peaks->second.empty()){

    const auto& pk=peaks->second;

    const auto itDur=s->durationCache.find(c.filename);

    const double sourceDuration=itDur==s->durationCache.end()?1.0:itDur->second;

    cairo_set_source_rgba(cr,.93,.97,1,.75);

    const int first=std::max(0,static_cast<int>(x)),last=std::min(width,static_cast<int>(x+ww));

    for(int col=first;col<last;++col){

     const double srcSec=c.sourceOffsetSeconds+((col-x)/s->zoom);

     const auto index=static_cast<std::size_t>(std::clamp(srcSec/std::max(.001,sourceDuration),0.0,.999999)*pk.size());

     const auto peak=pk[index];

     const double cy=y+46,top=cy-peak.max*16,bottom=cy-peak.min*16;

     cairo_rectangle(cr,col,top,1,std::max(1.0,bottom-top));

    }

    cairo_fill(cr);

   }

  }

  for(const auto& n:track.notes){

   const double x=16+n.atSeconds*s->zoom,ww=std::max(4.0,n.durationSeconds*s->zoom);

   cairo_set_source_rgb(cr,.39,.77,.64);cairo_rectangle(cr,x,y+44,ww,12);cairo_fill(cr);

  }

 }

 cairo_set_source_rgb(cr,1,.66,.34);const double playx=16+s->playhead*s->zoom;

 cairo_move_to(cr,playx,0);cairo_line_to(cr,playx,height);cairo_stroke(cr);

}

struct WaveformResult {

 std::string source;

 double seconds=0.0;

 std::vector<wenuystudio::Peak> peaks;

};

static void waveformReady(GObject* window,GAsyncResult* response,gpointer){

 GError* gerror=nullptr;

 auto* result=static_cast<WaveformResult*>(g_task_propagate_pointer(G_TASK(response),&gerror));

 auto* s=static_cast<Studio*>(g_object_get_data(window,"studio-state"));

 if(s && result){

  bool stillUsed=false;

  for(const auto& t:s->project.tracks){

   for(const auto& c:t.clips){

    if(c.filename==result->source){stillUsed=true;break;}

   }

   if(stillUsed)break;

  }

  if(stillUsed){

   s->durationCache[result->source]=result->seconds;

   s->peakCache[result->source]=std::move(result->peaks);

   if(s->timelineWidget){

#ifdef HAVE_WENUY

    wenuy_timeline_changed(s->timelineWidget);

#else

    gtk_widget_queue_draw(s->timelineWidget);

#endif

   }

  }

 }

 delete result;

 // Broken WAVs have empty previews; errors are reported via media check.

 if(gerror)g_error_free(gerror);

}

static void waveformWorker(GTask* task,gpointer,gpointer data,GCancellable*){

 auto* source=static_cast<std::string*>(data);

 try{

  auto* result=new WaveformResult;

  try{

   result->source=*source;

   wenuystudio::WaveReader reader(*source);

   result->seconds=double(reader.frames())/reader.rate();

   result->peaks=wenuystudio::waveformAccurate(*source,2048);

   g_task_return_pointer(task,result,[](gpointer p){delete static_cast<WaveformResult*>(p);});

  }catch(...){delete result;throw;}

 }catch(const std::exception& e){

  g_task_return_new_error(task,G_IO_ERROR,G_IO_ERROR_FAILED,"%s",e.what());

 }

}

static void cacheWaveforms(Studio* s){

 // Mark pending immediately to prevent duplicate jobs for repeated refreshes.

 // No disk scan or per-sample peak analysis runs on GTK's main thread.

 for(const auto& t:s->project.tracks)for(const auto& c:t.clips){

  if(s->peakCache.emplace(c.filename,std::vector<wenuystudio::Peak>{}).second){

   auto* task=g_task_new(G_OBJECT(s->window),nullptr,waveformReady,nullptr);

   g_task_set_task_data(task,new std::string(c.filename),[](gpointer p){delete static_cast<std::string*>(p);});

   g_task_run_in_thread(task,waveformWorker);

   g_object_unref(task);

  }

 }

}

#ifdef HAVE_WENUY

static void drawNativeWave(GtkSnapshot* snap,const wenuy::Clip* clip,float x,float y,

                           float w,float h,double secondsPerPixel,gpointer user){

 auto* s=static_cast<Studio*>(user);

 const auto it=s->peakCache.find(clip->source);

 if(it==s->peakCache.end()||it->second.empty())return;

 const auto dur=s->durationCache.find(clip->source);

 if(dur==s->durationCache.end()||dur->second<=0)return;

 const auto& peaks=it->second;

 const GdkRGBA ink{.92f,.97f,1.f,.8f};

 const int cols=std::max(0,std::min(static_cast<int>(w),2500));

 for(int i=0;i<cols;++i){

  const double sec=clip->offset+i*secondsPerPixel;

  const auto index=static_cast<std::size_t>(std::clamp(sec/dur->second,0.0,.999999)*peaks.size());

  const auto peak=peaks[index];

  const float cy=y+h*.52f,top=cy-peak.max*h*.34f,bottom=cy-peak.min*h*.34f;

  graphene_rect_t rect;

  graphene_rect_init(&rect,x+i,top,1.f,std::max(1.f,bottom-top));

  gtk_snapshot_append_color(snap,&ink,&rect);

 }

}

#endif

static void refreshTimeline(Studio* s){

 cacheWaveforms(s);

#ifdef HAVE_WENUY

 s->timeline.tracks.clear();s->timeline.clips.clear();s->gainByClipId.clear();

 for(const auto& track:s->project.tracks){

  wenuy::Track lane;lane.name=track.name;lane.muted=track.mute;lane.height=74;

  s->timeline.tracks.emplace_back(std::move(lane));

 }

 for(std::size_t i=0;i<s->project.tracks.size();++i){

  for(const auto& c:s->project.tracks[i].clips){

   wenuy::Clip vc;vc.track=static_cast<int>(i);vc.start=c.startSeconds;

   vc.offset=c.sourceOffsetSeconds;vc.length=clipDuration(s,c);

   vc.label=fs::path(c.filename).filename().string();vc.source=c.filename;

   const int clipId=s->timeline.addClip(vc);s->gainByClipId[clipId]=c.gain;

  }

 }

 s->timeline.playhead=s->playhead;

 wenuy_timeline_set_zoom(s->timelineWidget,s->zoom);

 wenuy_timeline_changed(s->timelineWidget);

#else

 gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(s->timelineWidget),static_cast<int>(s->project.durationSeconds*s->zoom)+50);

 gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(s->timelineWidget),std::max(140,static_cast<int>(s->project.tracks.size()*74+42)));

 gtk_widget_queue_draw(s->timelineWidget);

#endif

}

static void buildTracks(Studio* s){

 while(GtkWidget* child=gtk_widget_get_first_child(s->tracksBox))gtk_box_remove(GTK_BOX(s->tracksBox),child);

 for(std::size_t i=0;i<s->project.tracks.size();++i){

  auto& track=s->project.tracks[i];

  GtkWidget* panel=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);

  gtk_widget_set_margin_start(panel,10);gtk_widget_set_margin_end(panel,10);

  gtk_widget_set_margin_top(panel,8);gtk_widget_set_margin_bottom(panel,8);

  gtk_widget_add_css_class(panel,"studio-track");

  GtkWidget* title=gtk_label_new(track.name.c_str());gtk_label_set_xalign(GTK_LABEL(title),0);

  gtk_widget_add_css_class(title,"heading");gtk_box_append(GTK_BOX(panel),title);

  GtkWidget* row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);

  GtkWidget* mute=gtk_check_button_new_with_label(tr("Mute","Tyst"));

  gtk_check_button_set_active(GTK_CHECK_BUTTON(mute),track.mute);

  auto* ct=new TrackControl{s,i};

  g_object_set_data_full(G_OBJECT(mute),"track-control",ct,[](gpointer p){delete static_cast<TrackControl*>(p);});

  g_signal_connect(mute,"toggled",G_CALLBACK(+[](GtkCheckButton* toggle,gpointer context){

   auto* c=static_cast<TrackControl*>(context);checkpoint(c->studio);c->studio->project.tracks[c->index].mute=gtk_check_button_get_active(toggle);

   markChanged(c->studio);refreshTimeline(c->studio);

  }),ct);

  gtk_box_append(GTK_BOX(row),mute);

  GtkWidget* solo=gtk_check_button_new_with_label(tr("Solo","Solo"));

  gtk_check_button_set_active(GTK_CHECK_BUTTON(solo),track.solo);

  auto* cs=new TrackControl{s,i};

  g_object_set_data_full(G_OBJECT(solo),"track-control",cs,[](gpointer p){delete static_cast<TrackControl*>(p);});

  g_signal_connect(solo,"toggled",G_CALLBACK(+[](GtkCheckButton* toggle,gpointer context){

   auto* c=static_cast<TrackControl*>(context);checkpoint(c->studio);

   c->studio->project.tracks[c->index].solo=gtk_check_button_get_active(toggle);

   markChanged(c->studio);

  }),cs);

  gtk_box_append(GTK_BOX(row),solo);

  GtkWidget* count=gtk_label_new((std::to_string(track.clips.size())+tr(" clips"," klipp")).c_str());

  gtk_widget_add_css_class(count,"dim-label");gtk_box_append(GTK_BOX(row),count);

  gtk_box_append(GTK_BOX(panel),row);

  GtkWidget* vol=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,2,.05);

  gtk_range_set_value(GTK_RANGE(vol),track.gain);gtk_scale_set_draw_value(GTK_SCALE(vol),FALSE);

  gtk_widget_set_tooltip_text(vol,tr("Track volume","Spårvolym"));

  auto* cv=new TrackControl{s,i};

  g_object_set_data_full(G_OBJECT(vol),"track-control",cv,[](gpointer p){delete static_cast<TrackControl*>(p);});

  g_signal_connect(vol,"value-changed",G_CALLBACK(+[](GtkRange* v,gpointer context){

   auto* c=static_cast<TrackControl*>(context);checkpoint(c->studio);c->studio->project.tracks[c->index].gain=static_cast<float>(gtk_range_get_value(v));

   markChanged(c->studio);

  }),cv);

  gtk_box_append(GTK_BOX(panel),vol);

  GtkWidget* panRow=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);

  GtkWidget* panLabel=gtk_label_new(tr("Pan","Pan"));

  gtk_box_append(GTK_BOX(panRow),panLabel);

  GtkWidget* pan=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-1,1,.05);

  gtk_widget_set_hexpand(pan,TRUE);

  gtk_scale_set_draw_value(GTK_SCALE(pan),FALSE);

  gtk_range_set_value(GTK_RANGE(pan),track.pan);

  gtk_widget_set_tooltip_text(pan,tr("Left / right balance","Balans vänster / höger"));

  auto* cp=new TrackControl{s,i};

  g_object_set_data_full(G_OBJECT(pan),"track-control",cp,[](gpointer p){delete static_cast<TrackControl*>(p);});

  g_signal_connect(pan,"value-changed",G_CALLBACK(+[](GtkRange* r,gpointer context){

   auto* c=static_cast<TrackControl*>(context);checkpoint(c->studio);

   c->studio->project.tracks[c->index].pan=static_cast<float>(gtk_range_get_value(r));

   markChanged(c->studio);

  }),cp);

  gtk_box_append(GTK_BOX(panRow),pan);

  gtk_box_append(GTK_BOX(panel),panRow);

  gtk_box_append(GTK_BOX(s->tracksBox),panel);

 }

}

static void refresh(Studio* s){

 recalcDuration(s);

 s->updatingControls=true;

 if(s->masterScale)gtk_range_set_value(GTK_RANGE(s->masterScale),s->project.masterGain);

 s->updatingControls=false;

 buildTracks(s);refreshTimeline(s);status(s);

}

#ifdef HAVE_WENUY

static void clipsChanged(GtkWidget*,gpointer user){

 auto* s=static_cast<Studio*>(user);

 checkpoint(s);

 for(auto& t:s->project.tracks)t.clips.clear();

 for(const auto& c:s->timeline.clips){

  if(c.track<0||static_cast<std::size_t>(c.track)>=s->project.tracks.size())continue;

  if(c.start<0||c.length<=0||c.offset<0||c.start>kMaxLength)continue;

  const auto it=s->gainByClipId.find(c.id);

  const float gain=it==s->gainByClipId.end()?1.0f:it->second;

  s->project.tracks[static_cast<std::size_t>(c.track)].clips.push_back({c.source,c.start,c.offset,c.length,gain});

 }

 recalcDuration(s);buildTracks(s);markChanged(s);

}

static void playheadMoved(GtkWidget*,double seconds,gpointer user){

 auto* s=static_cast<Studio*>(user);s->playhead=std::max(0.0,seconds);

}

#endif

static void addTrack(Studio* s){

 if(s->project.tracks.size()>=128){notify(s,tr("Maximum 128 tracks","Högst 128 spår"));return;}

 checkpoint(s);

 wenuystudio::Track track;track.name=tr("Track ","Spår ")+std::to_string(s->project.tracks.size()+1);

 s->project.tracks.push_back(std::move(track));refresh(s);markChanged(s);

}

static void addNote(Studio* s){

 if(s->project.tracks.empty())addTrack(s);

 checkpoint(s);s->project.tracks.front().notes.push_back({60,s->playhead,.5,.75f});

 refresh(s);markChanged(s);

 notify(s,tr("C4 note added on first track","Ton C4 tillagd på första spåret"));

}

static bool hasExtension(const std::string& path,const std::string& ext){return path.size()>=ext.size() && path.compare(path.size()-ext.size(),ext.size(),ext)==0;}

static void save(Studio* s,const std::string& path){

 const std::string oldFile=s->projectFile;

 wenuystudio::saveProject(s->project,path);

 s->projectFile=path;s->dirty=false;s->autosaveBlocked=false;s->autosavedVersion=s->changeVersion;

 // Cleanup is best effort: a failed cleanup must never make an already-saved project appear unsaved.

 try{wenuystudio::discardRecovery(oldFile);if(oldFile!=path)wenuystudio::discardRecovery(path);}

 catch(const std::exception& e){notify(s,std::string(tr("Project saved, but recovery cleanup failed: ","Projektet sparades, men återställningskopian kunde inte rensas: "))+e.what());}

 status(s);notify(s,tr("Project saved","Projektet sparat"));

}

static void exported(GObject* source,GAsyncResult* response,gpointer){

 auto* s=static_cast<Studio*>(g_object_get_data(source,"studio-state"));

 if(!s)return;

 GError* gerror=nullptr;

 const bool ok=g_task_propagate_boolean(G_TASK(response),&gerror);

 s->exporting=false;

 if(s->renderButton)gtk_widget_set_sensitive(s->renderButton,TRUE);

 status(s);

 if(ok)notify(s,tr("WAV mix exported","WAV-mix exporterad"));

 else if(gerror){notify(s,gerror->message);g_clear_error(&gerror);}

}

static void exportWorker(GTask* task,gpointer,gpointer taskData,GCancellable*){

 auto* request=static_cast<ExportRequest*>(taskData);

 try{wenuystudio::render(request->project,request->output);g_task_return_boolean(task,TRUE);}

 catch(const std::exception& e){g_task_return_new_error(task,G_IO_ERROR,G_IO_ERROR_FAILED,"%s",e.what());}

}

static void startExport(Studio* s,const std::string& filepath){

 if(s->exporting)return;

 s->exporting=true;gtk_widget_set_sensitive(s->renderButton,FALSE);status(s);

 auto* task=g_task_new(G_OBJECT(s->window),nullptr,exported,nullptr);

 auto* req=new ExportRequest{s->project,filepath};

 g_task_set_task_data(task,req,[](gpointer p){delete static_cast<ExportRequest*>(p);});

 g_task_run_in_thread(task,exportWorker);g_object_unref(task);

}

// The device list is read only. It does NOT claim the microphone/speakers

// were successfully opened: that requires a hardware test on WenuyOS.

static void showAudioDevices(Studio* s){

 std::string message;

 try{

  const auto report=wenuystudio::queryAudioDevices();

  message=std::to_string(report.devices.size())+tr(" audio devices found\n", " ljudenheter hittades\n");

  for(const auto& d:report.devices){

   message+="["+std::to_string(d.index)+"] "+d.name+" ("+d.hostApi+")";

   if(d.defaultInput)message+=tr(" · default microphone", " · standardmikrofon");

   if(d.defaultOutput)message+=tr(" · default output", " · standardutgång");

   message+="\n";

  }

  if(report.defaultInput<0)message+=tr("\nNo default microphone. Check audio permissions.","\nIngen standardmikrofon. Kontrollera ljudbehörighet.");

  if(report.defaultOutput<0)message+=tr("\nNo default speaker. Check audio settings.","\nIngen standardhögtalare. Kontrollera ljudinställningarna.");

  message+=tr("\nListing devices does not verify that recording works.","\nListning verifierar inte att inspelning fungerar.");

 }catch(const std::exception& e){

  message=std::string(tr("Audio devices unavailable: ","Ljudenheter saknas: "))+e.what();

 }

 GtkWidget* dialog=gtk_message_dialog_new(GTK_WINDOW(s->window),GTK_DIALOG_MODAL,

  GTK_MESSAGE_INFO,GTK_BUTTONS_CLOSE,"%s",tr("Audio devices","Ljudenheter"));

 gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),"%s",message.c_str());

 g_signal_connect_swapped(dialog,"response",G_CALLBACK(gtk_window_destroy),dialog);

 gtk_window_present(GTK_WINDOW(dialog));

}

static void deviceProbeDone(GObject* source,GAsyncResult* response,gpointer){

 auto* s=static_cast<Studio*>(g_object_get_data(source,"studio-state"));

 GError* error=nullptr;

 auto* probe=static_cast<wenuystudio::AudioProbeReport*>(g_task_propagate_pointer(G_TASK(response),&error));

 if(!s){delete probe;if(error)g_error_free(error);return;}

 if(error){notify(s,error->message);g_error_free(error);return;}

 const std::string msg=std::string(tr("Output: ","Utgång: "))+

  (probe->outputOpened?tr("stream opened","ström öppnades"):probe->outputError)+"\n"+

  tr("Microphone: ","Mikrofon: ")+

  (probe->inputOpened?tr("stream opened","ström öppnades"):probe->inputError)+"\n"+

  tr("Only tests opening streams; not sound quality or captured audio.",

     "Testar bara att öppna strömmar, inte ljudkvalitet eller inspelat ljud.");

 GtkWidget* dialog=gtk_message_dialog_new(GTK_WINDOW(s->window),GTK_DIALOG_MODAL,

  GTK_MESSAGE_INFO,GTK_BUTTONS_CLOSE,"%s",tr("Audio device test","Ljudenhetstest"));

 gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),"%s",msg.c_str());

 g_signal_connect_swapped(dialog,"response",G_CALLBACK(gtk_window_destroy),dialog);

 gtk_window_present(GTK_WINDOW(dialog));

 delete probe;

}

static void runProbeWorker(GTask* task,gpointer,gpointer,GCancellable*){

 try{

  auto* result=new wenuystudio::AudioProbeReport(wenuystudio::probeDefaultAudioDevices());

  g_task_return_pointer(task,result,[](gpointer p){delete static_cast<wenuystudio::AudioProbeReport*>(p);});

 }catch(const std::exception& e){

  g_task_return_new_error(task,G_IO_ERROR,G_IO_ERROR_FAILED,"%s",e.what());

 }

}

static void runAudioProbe(Studio* s){

 notify(s,tr("Checking audio streams…","Kontrollerar ljudströmmar…"));

 auto* task=g_task_new(G_OBJECT(s->window),nullptr,deviceProbeDone,nullptr);

 g_task_run_in_thread(task,runProbeWorker);g_object_unref(task);

}

static void stopAudio(Studio* s);

// Autosave only between recording/export operations. No user project is overwritten.

static gboolean autosaveTick(gpointer user){

 auto* s=static_cast<Studio*>(user);

 if(!s->dirty||s->autosaveBlocked||s->recording.active()||s->wasRecording||s->exporting

    ||s->changeVersion==s->autosavedVersion)return G_SOURCE_CONTINUE;

 try{

  wenuystudio::saveRecovery(s->project,s->projectFile);

  s->autosavedVersion=s->changeVersion;s->autosaveErrorShown=false;status(s);

 }catch(const std::exception& e){

  if(!s->autosaveErrorShown){notify(s,std::string(tr("Autosave failed: ","Autosparning misslyckades: "))+e.what());s->autosaveErrorShown=true;}

 }

 return G_SOURCE_CONTINUE;

}

static void checkMedia(Studio* s){

 const auto issues=wenuystudio::inspectMedia(s->project);

 std::string message;

 if(issues.empty())message=tr("All audio clips are accessible.","Alla ljudklipp är tillgängliga.");

 else {

  message=std::to_string(issues.size())+tr(" audio problems found:"," problem med ljudfiler hittades:");

  for(std::size_t i=0;i<std::min<std::size_t>(issues.size(),5);++i){

   const auto& issue=issues[i];

   message+="\n"+std::to_string(issue.trackIndex+1)+"/"+std::to_string(issue.clipIndex+1)+" "

    +wenuystudio::mediaIssueName(issue.kind)+": "+fs::path(issue.filename).filename().string();

  }

  if(issues.size()>5)message+=tr("\nMore problems found. Run 'wenuy-studio-cli check' for details.",

   "\nFler fel finns. Kör 'wenuy-studio-cli check' för detaljer.");

 }

 GtkWidget* dialog=gtk_message_dialog_new(GTK_WINDOW(s->window),GTK_DIALOG_MODAL,

  issues.empty()?GTK_MESSAGE_INFO:GTK_MESSAGE_WARNING,GTK_BUTTONS_CLOSE,

  "%s",tr("Audio-file check","Kontroll av ljudfiler"));

 gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),"%s",message.c_str());

 g_signal_connect_swapped(dialog,"response",G_CALLBACK(gtk_window_destroy),dialog);

 gtk_window_present(GTK_WINDOW(dialog));

}

static void setOpenedProject(Studio* s,wenuystudio::Project p,const std::string& path,bool recovered){

 stopAudio(s);

 s->project=std::move(p);s->projectFile=path;s->dirty=recovered;

 s->history.clear();s->durationCache.clear();s->peakCache.clear();s->playhead=0;

 s->autosaveBlocked=false;s->autosaveErrorShown=false;

 ++s->changeVersion;s->autosavedVersion=s->changeVersion;

 refresh(s);

 const auto issues=wenuystudio::inspectMedia(s->project);

 if(!issues.empty())notify(s,std::to_string(issues.size())+tr(" audio clips need attention. Use the audio-file check button.",

  " ljudklipp behöver åtgärdas. Använd knappen för ljudfilskontroll."));

 else notify(s,recovered?tr("Recovery loaded. Save to keep changes.","Återställningskopian öppnades. Spara för att behålla ändringarna.")

  :tr("Project opened","Projektet öppnat"));

}

struct RecoveryRequest {Studio* studio;std::string path;};

static void showRecoveryPrompt(Studio* s,const std::string& path){

 s->autosaveBlocked=(path==s->projectFile);

 GtkWidget* dialog=gtk_message_dialog_new(GTK_WINDOW(s->window),GTK_DIALOG_MODAL,

  GTK_MESSAGE_QUESTION,GTK_BUTTONS_NONE,"%s",tr("Recovery copy found","En återställningskopia finns"));

 gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),"%s",

  tr("An autosaved version is available. Restore it, or discard it and continue with the saved version. Cancel leaves the copy untouched.",

  "En autosparad version finns. Återställ den eller kasta kopian och fortsätt med den vanliga versionen. Avbryt behåller kopian."));

 gtk_dialog_add_buttons(GTK_DIALOG(dialog),tr("Cancel","Avbryt"),GTK_RESPONSE_CANCEL,

  tr("Discard copy","Kasta kopian"),GTK_RESPONSE_REJECT,

  tr("Restore","Återställ"),GTK_RESPONSE_ACCEPT,nullptr);

 auto* req=new RecoveryRequest{s,path};

 g_signal_connect_data(dialog,"response",G_CALLBACK(+[](GtkDialog* d,int response,gpointer data){

  const auto* r=static_cast<RecoveryRequest*>(data);auto* st=r->studio;

  const std::string filename=r->path;

  gtk_window_destroy(GTK_WINDOW(d));

  try{

   if(response==GTK_RESPONSE_ACCEPT){setOpenedProject(st,wenuystudio::loadRecovery(filename),filename,true);}

   else if(response==GTK_RESPONSE_REJECT){

    // Validate the existing project BEFORE deleting its recovery fallback.

    if(!filename.empty()){

     auto original=wenuystudio::loadProject(filename);

     wenuystudio::discardRecovery(filename);

     setOpenedProject(st,std::move(original),filename,false);

    }else{wenuystudio::discardRecovery(filename);st->autosaveBlocked=false;status(st);}

   }else if(filename!=st->projectFile)st->autosaveBlocked=false;

  }catch(const std::exception& e){error(st,e);}

 }),req,[](gpointer p,GClosure*){delete static_cast<RecoveryRequest*>(p);},GConnectFlags(0));

 gtk_window_present(GTK_WINDOW(dialog));

}

static void chosen(GtkNativeDialog* dialog,gint response,gpointer user){

 auto* s=static_cast<Studio*>(user);

 const Pick choice=static_cast<Pick>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog),"pick")));

 if(response==GTK_RESPONSE_ACCEPT){

  GFile* f=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));

  if(f){

   char* path=g_file_get_path(f);

   if(path)try{

    std::string name(path);

    if(choice==Pick::OpenProject){

     if(s->dirty){notify(s,tr("Save current project before opening another","Spara projektet innan du öppnar ett annat"));}

     else if(s->wasRecording){notify(s,tr("Stop recording before opening another project","Avsluta inspelningen innan du öppnar ett annat projekt"));}

     else if(wenuystudio::hasRecovery(name))showRecoveryPrompt(s,name);

     else setOpenedProject(s,wenuystudio::loadProject(name),name,false);

    } else if(choice==Pick::SaveProject){

     if(!hasExtension(name,".wstudio"))name+=".wstudio";

     save(s,name);

    } else if(choice==Pick::ImportAudio){

     wenuystudio::WaveReader rd(name);

     checkpoint(s);

     wenuystudio::Track track;track.name=fs::path(name).stem().string();

     track.clips.push_back({name,s->playhead,0,0,1.0f});

     s->project.tracks.push_back(std::move(track));

     s->project.durationSeconds=std::min(kMaxLength,std::max(s->project.durationSeconds,s->playhead+static_cast<double>(rd.frames())/rd.rate()+1));

     refresh(s);markChanged(s);notify(s,tr("Sound file imported","Ljudfil importerad"));

    } else if(choice==Pick::RecordAudio){

     if(!hasExtension(name,".wav"))name+=".wav";

     s->playback.stop();s->wasPlaying=false;

     s->recordAt=s->playhead;

     s->recording.start(name,s->project.sampleRate);

     s->recordTarget=name;s->wasRecording=true;

     status(s);notify(s,tr("Recording started. Press Stop to finish.","Inspelning pågår. Tryck Stopp när du är klar."));

    } else if(choice==Pick::ExportAudio){

     if(!hasExtension(name,".wav"))name+=".wav";

     startExport(s,name);

    }

   }catch(const std::exception& e){error(s,e);}

   g_free(path);g_object_unref(f);

  }

 }

 g_object_unref(dialog);

}

static void choose(Studio* s,Pick op){

 const bool saving=op==Pick::SaveProject||op==Pick::ExportAudio||op==Pick::RecordAudio;

 const char* title=op==Pick::OpenProject?tr("Open project","Öppna projekt"):

  op==Pick::SaveProject?tr("Save project","Spara projekt"):

  op==Pick::ImportAudio?tr("Import WAV","Importera WAV"):

  op==Pick::RecordAudio?tr("Record to WAV","Spela in till WAV"):tr("Export WAV mix","Exportera WAV-mix");

 auto* dialog=gtk_file_chooser_native_new(title,GTK_WINDOW(s->window),

  saving?GTK_FILE_CHOOSER_ACTION_SAVE:GTK_FILE_CHOOSER_ACTION_OPEN,

  saving?tr("Save","Spara"):tr("Open","Öppna"),tr("Cancel","Avbryt"));

 g_object_set_data(G_OBJECT(dialog),"pick",GINT_TO_POINTER(static_cast<int>(op)));

 if(saving)gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),op==Pick::ExportAudio?"mix.wav":op==Pick::RecordAudio?"inspelning.wav":"musikprojekt.wstudio");

 g_signal_connect(dialog,"response",G_CALLBACK(chosen),s);

 gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));

}

static GtkWidget* action(Studio* s,GtkWidget* bar,const char* icon,const char* hint,void (*callback)(Studio*)){

 GtkWidget* btn=gtk_button_new_from_icon_name(icon);

 gtk_widget_set_tooltip_text(btn,hint);

 g_signal_connect_swapped(btn,"clicked",G_CALLBACK(callback),s);

 gtk_box_append(GTK_BOX(bar),btn);return btn;

}

static void stopAudio(Studio* s){

 s->playback.stop();s->wasPlaying=false;

 if(s->wasRecording){

  s->recording.stop();s->wasRecording=false;

  const auto message=s->recording.error();

  if(!message.empty())notify(s,message);

  else if(s->recording.frames()>0){

   if(s->project.tracks.size()>=128){notify(s,tr("Maximum 128 tracks; recording saved as WAV", "Högst 128 spår; inspelningen finns kvar som WAV"));return;}

   checkpoint(s);

   wenuystudio::Track t;t.name=fs::path(s->recordTarget).stem().string();

   t.clips.push_back({s->recordTarget,s->recordAt,0,0,1});

   s->project.tracks.push_back(std::move(t));

   refresh(s);markChanged(s);

   notify(s,tr("Recording added as new track","Inspelning tillagd som ett nytt spår"));

  }

 }

 status(s);refreshTimeline(s);

}

static void playAudio(Studio* s){

 if(s->wasRecording){notify(s,tr("Stop recording first","Avsluta inspelningen först"));return;}

 try{

  s->playback.start(s->project,s->playhead,s->bufferPreset);

  s->wasPlaying=true;

  notify(s,tr("Buffered playback started. Mix edits apply on next Play.","Buffrad uppspelning startad. Mixerändringar gäller från nästa Spela."));

  status(s);

 }catch(const std::exception& e){error(s,e);}

}

static void undoEdit(Studio* s){

 if(s->recording.active()){notify(s,tr("Stop recording before editing","Avsluta inspelning före redigering"));return;}

 if(s->history.undo(s->project)){refresh(s);markChanged(s);}

 else notify(s,tr("Nothing to undo","Inget att ångra"));

}

static void redoEdit(Studio* s){

 if(s->recording.active()){notify(s,tr("Stop recording before editing","Avsluta inspelning före redigering"));return;}

 if(s->history.redo(s->project)){refresh(s);markChanged(s);}

 else notify(s,tr("Nothing to redo","Inget att göra om"));

}

static gboolean updateTransport(gpointer user){

 auto* s=static_cast<Studio*>(user);

 if(s->wasPlaying){

  if(s->playback.active()){

   if(s->meterLabel){

    const auto percent=[](float peak){return std::to_string(static_cast<int>(std::round(peak*100.0f)));};

    const auto d=s->playback.diagnostics();

    const std::string message="L "+percent(s->playback.peakLeft())+"% · R "+percent(s->playback.peakRight())+"% · "+

     tr("Buffer ","Buffert ")+std::to_string(static_cast<int>(std::round(d.bufferedMilliseconds)))+" ms · "+

     tr("Underruns ","Ljudavbrott ")+std::to_string(d.underruns);

    gtk_label_set_text(GTK_LABEL(s->meterLabel),message.c_str());

   }

   s->playhead=std::min(s->project.durationSeconds,s->playback.positionSeconds());

#ifdef HAVE_WENUY

   s->timeline.playhead=s->playhead;

#endif

   if(s->timelineWidget){

#ifdef HAVE_WENUY

    wenuy_timeline_changed(s->timelineWidget);

#else

    gtk_widget_queue_draw(s->timelineWidget);

#endif

   }

  }else{

   s->wasPlaying=false;

   if(s->meterLabel)gtk_label_set_text(GTK_LABEL(s->meterLabel),"L 0% · R 0%");

   const auto message=s->playback.error();

   if(!message.empty())notify(s,message);

   const auto d=s->playback.diagnostics();

   if(d.underruns>0)notify(s,std::string(tr("Playback underruns: ","Ljudavbrott under uppspelning: "))+std::to_string(d.underruns)+

    tr(". Try Safe buffer mode.",". Prova Stabil buffert."));

   status(s);

  }

 }

 if(s->wasRecording&&!s->recording.active())stopAudio(s);

 return G_SOURCE_CONTINUE;

}

static void buildWindow(GtkApplication* app){

 auto* s=new Studio;

 s->project.durationSeconds=5;

 wenuystudio::Track initial;initial.name=tr("Track 1","Spår 1");s->project.tracks.push_back(std::move(initial));

 s->window=app::windowNew(app,"Wenuy Studio");

 g_object_set_data_full(G_OBJECT(s->window),"studio-state",s,[](gpointer p){delete static_cast<Studio*>(p);});

 gtk_window_set_default_size(GTK_WINDOW(s->window),1050,690);

 GtkWidget* bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);

 gtk_widget_set_margin_start(bar,12);gtk_widget_set_margin_end(bar,12);

 gtk_widget_set_margin_top(bar,7);gtk_widget_set_margin_bottom(bar,7);

 action(s,bar,"document-new-symbolic",tr("New project","Nytt projekt"),+[](Studio* st){

  if(st->dirty){notify(st,tr("Save your current project first","Spara det aktuella projektet först"));return;}

  stopAudio(st);st->project=wenuystudio::Project{};st->project.durationSeconds=5;

  wenuystudio::Track t;t.name=tr("Track 1","Spår 1");st->project.tracks.push_back(std::move(t));

  st->projectFile.clear();st->history.clear();st->durationCache.clear();st->peakCache.clear();st->playhead=0;st->dirty=false;st->autosaveBlocked=false;refresh(st);

  if(wenuystudio::hasRecovery(""))showRecoveryPrompt(st,"");

 });

 action(s,bar,"document-open-symbolic",tr("Open project","Öppna projekt"),+[](Studio* st){choose(st,Pick::OpenProject);});

 action(s,bar,"document-save-symbolic",tr("Save project","Spara projekt"),+[](Studio* st){

  try{if(st->projectFile.empty())choose(st,Pick::SaveProject);else save(st,st->projectFile);}

  catch(const std::exception& e){error(st,e);}

 });

 action(s,bar,"document-save-as-symbolic",tr("Save as…","Spara som…"),+[](Studio* st){choose(st,Pick::SaveProject);});

 GtkWidget* separator=gtk_separator_new(GTK_ORIENTATION_VERTICAL);gtk_box_append(GTK_BOX(bar),separator);

 action(s,bar,"list-add-symbolic",tr("Add track","Lägg till spår"),addTrack);

 action(s,bar,"document-import-symbolic",tr("Import WAV file","Importera WAV-fil"),+[](Studio* st){choose(st,Pick::ImportAudio);});

 action(s,bar,"dialog-warning-symbolic",tr("Check missing or damaged audio","Kontrollera saknade eller skadade ljudfiler"),checkMedia);

 action(s,bar,"media-playback-start-symbolic",tr("Play","Spela"),playAudio);

 action(s,bar,"media-playback-stop-symbolic",tr("Stop playback or recording","Stoppa uppspelning eller inspelning"),stopAudio);

 action(s,bar,"audio-card-symbolic",tr("Audio devices and troubleshooting","Ljudenheter och felsökning"),showAudioDevices);

 action(s,bar,"emblem-ok-symbolic",tr("Test opening microphone and speaker streams","Testa att öppna mikrofon och högtalare"),runAudioProbe);

 s->recordButton=action(s,bar,"media-record-symbolic",tr("Record microphone","Spela in mikrofon"),+[](Studio* st){

  if(st->wasRecording)stopAudio(st);else choose(st,Pick::RecordAudio);

 });

 action(s,bar,"edit-undo-symbolic",tr("Undo","Ångra"),undoEdit);

 action(s,bar,"edit-redo-symbolic",tr("Redo","Gör om"),redoEdit);

 action(s,bar,"list-add-symbolic",tr("Add a C4 note at playhead","Lägg till en C4-ton vid markören"),addNote);

 GtkWidget* expander=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);

 gtk_widget_set_hexpand(expander,TRUE);

 gtk_widget_set_halign(expander,GTK_ALIGN_END);

 GtkWidget* hint=gtk_label_new(tr("Playback · Recording · WAV","Spela · Spela in · WAV"));

 gtk_widget_add_css_class(hint,"dim-label");gtk_box_append(GTK_BOX(expander),hint);

 s->renderButton=gtk_button_new_with_label(tr("Export mix","Exportera mix"));

 gtk_widget_add_css_class(s->renderButton,"suggested-action");

 g_signal_connect_swapped(s->renderButton,"clicked",G_CALLBACK(+[](Studio* st){choose(st,Pick::ExportAudio);}),s);

 gtk_box_append(GTK_BOX(expander),s->renderButton);gtk_box_append(GTK_BOX(bar),expander);

 GtkWidget* toolbarScroll=gtk_scrolled_window_new();

 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(toolbarScroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_NEVER);

 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(toolbarScroll),bar);

 app::windowAddTopBar(s->window,toolbarScroll);

 GtkWidget* mainBox=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);

 GtkWidget* caption=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);

 gtk_widget_set_margin_start(caption,17);gtk_widget_set_margin_end(caption,17);

 gtk_widget_set_margin_top(caption,12);gtk_widget_set_margin_bottom(caption,12);

 s->titleLabel=gtk_label_new(tr("Untitled project","Namnlöst projekt"));

 gtk_widget_add_css_class(s->titleLabel,"title-2");gtk_label_set_xalign(GTK_LABEL(s->titleLabel),0);

 gtk_widget_set_hexpand(s->titleLabel,TRUE);gtk_box_append(GTK_BOX(caption),s->titleLabel);

 GtkWidget* latencyLabel=gtk_label_new(tr("Audio buffer","Ljudbuffert"));

 gtk_box_append(GTK_BOX(caption),latencyLabel);

 const char* const presets[]={tr("Low latency","Låg latens"),tr("Balanced","Balanserad"),tr("Safe","Stabil"),nullptr};

 s->bufferDropdown=gtk_drop_down_new_from_strings(presets);

 gtk_drop_down_set_selected(GTK_DROP_DOWN(s->bufferDropdown),1);

 gtk_widget_set_tooltip_text(s->bufferDropdown,

  tr("Low for fast response; Safe if sound stutters. Takes effect at next Play.",

     "Låg ger snabb respons; Stabil vid ljudhack. Gäller från nästa Spela."));

 g_signal_connect(s->bufferDropdown,"notify::selected",G_CALLBACK(+[](GtkDropDown* drop,GParamSpec*,gpointer user){

  auto* st=static_cast<Studio*>(user);

  switch(gtk_drop_down_get_selected(drop)){

   case 0:st->bufferPreset=wenuystudio::BufferPreset::Low;break;

   case 2:st->bufferPreset=wenuystudio::BufferPreset::Safe;break;

   default:st->bufferPreset=wenuystudio::BufferPreset::Balanced;break;

  }

 }),s);

 gtk_box_append(GTK_BOX(caption),s->bufferDropdown);

 GtkWidget* zoomLabel=gtk_label_new(tr("Zoom","Zoom"));gtk_box_append(GTK_BOX(caption),zoomLabel);

 GtkWidget* scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,24,160,4);

 gtk_range_set_value(GTK_RANGE(scale),s->zoom);gtk_widget_set_size_request(scale,105,-1);

 g_signal_connect(scale,"value-changed",G_CALLBACK(+[](GtkRange* r,gpointer user){

  auto* st=static_cast<Studio*>(user);st->zoom=gtk_range_get_value(r);refreshTimeline(st);

 }),s);

 gtk_box_append(GTK_BOX(caption),scale);

 gtk_box_append(GTK_BOX(mainBox),caption);

 GtkWidget* split=gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

 gtk_widget_set_vexpand(split,TRUE);

 GtkWidget* leftScroll=gtk_scrolled_window_new();gtk_widget_set_size_request(leftScroll,220,-1);

 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(leftScroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);

 s->tracksBox=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);

 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(leftScroll),s->tracksBox);

 gtk_paned_set_start_child(GTK_PANED(split),leftScroll);

#ifdef HAVE_WENUY

 s->timelineWidget=wenuy_timeline_new(&s->timeline);

 wenuy_timeline_set_clip_draw_func(s->timelineWidget,drawNativeWave,s);

 g_signal_connect(s->timelineWidget,"clips-changed",G_CALLBACK(clipsChanged),s);

 g_signal_connect(s->timelineWidget,"playhead-moved",G_CALLBACK(playheadMoved),s);

#else

 s->timelineWidget=gtk_drawing_area_new();

 gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(s->timelineWidget),drawFallback,s,nullptr);

 auto* click=gtk_gesture_click_new();

 g_signal_connect(click,"pressed",G_CALLBACK(+[](GtkGestureClick*,int,double x,double,gpointer user){

  auto* st=static_cast<Studio*>(user);st->playhead=std::clamp((x-16)/st->zoom,0.0,st->project.durationSeconds);

  gtk_widget_queue_draw(st->timelineWidget);

 }),s);

 gtk_widget_add_controller(s->timelineWidget,GTK_EVENT_CONTROLLER(click));

#endif

 GtkWidget* rightScroll=gtk_scrolled_window_new();

 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rightScroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rightScroll),s->timelineWidget);

 gtk_paned_set_end_child(GTK_PANED(split),rightScroll);

 gtk_paned_set_position(GTK_PANED(split),245);

 gtk_box_append(GTK_BOX(mainBox),split);

 GtkWidget* foot=gtk_label_new(tr("Tip: Import a WAV, arrange the clips and export a mix.",

  "Tips: Importera en WAV-fil, ordna klippen och exportera en mix."));

 gtk_widget_set_margin_top(foot,9);gtk_widget_set_margin_bottom(foot,9);

 gtk_widget_add_css_class(foot,"dim-label");

 gtk_box_append(GTK_BOX(mainBox),foot);

 app::windowSetContent(s->window,mainBox);

 g_signal_connect(s->window,"close-request",G_CALLBACK(+[](GtkWindow* win,gpointer userdata)->gboolean{

  auto* st=static_cast<Studio*>(userdata);

  if(!st->dirty){stopAudio(st);return FALSE;}

  GtkWidget* question=gtk_message_dialog_new(win,GTK_DIALOG_MODAL,GTK_MESSAGE_WARNING,

   GTK_BUTTONS_NONE,"%s",tr("Unsaved changes. Close without saving?","Osparade ändringar. Stäng utan att spara?"));

  gtk_dialog_add_buttons(GTK_DIALOG(question),tr("Cancel","Avbryt"),GTK_RESPONSE_CANCEL,

   tr("Close without saving","Stäng utan att spara"),GTK_RESPONSE_ACCEPT,nullptr);

  g_signal_connect(question,"response",G_CALLBACK(+[](GtkDialog* dlg,int response,gpointer user){

   auto* current=static_cast<Studio*>(user);

   gtk_window_destroy(GTK_WINDOW(dlg));

   if(response==GTK_RESPONSE_ACCEPT){stopAudio(current);current->dirty=false;gtk_window_close(GTK_WINDOW(current->window));}

  }),st);

  gtk_window_present(GTK_WINDOW(question));return TRUE;

 }),s);

 GtkEventController* shortcuts=gtk_event_controller_key_new();

 g_signal_connect(shortcuts,"key-pressed",G_CALLBACK(+[](GtkEventControllerKey*,guint key,guint,GdkModifierType flags,gpointer user)->gboolean{

  if(!(flags&GDK_CONTROL_MASK))return FALSE;

  auto* st=static_cast<Studio*>(user);

  switch(gdk_keyval_to_lower(key)){

   case 's':try{if(!st->projectFile.empty())save(st,st->projectFile);else choose(st,Pick::SaveProject);}catch(const std::exception& e){error(st,e);}return TRUE;

   case 'o':choose(st,Pick::OpenProject);return TRUE;

   case 'i':choose(st,Pick::ImportAudio);return TRUE;

   case 'z':if(flags&GDK_SHIFT_MASK)redoEdit(st);else undoEdit(st);return TRUE;

   case 'y':redoEdit(st);return TRUE;

   default:return FALSE;

  }

 }),s);

 gtk_widget_add_controller(s->window,shortcuts);

 refresh(s);

 s->tickSource=g_timeout_add(120,updateTransport,s);

 s->autosaveSource=g_timeout_add_seconds(45,autosaveTick,s);

 gtk_window_present(GTK_WINDOW(s->window));

 if(wenuystudio::hasRecovery(""))showRecoveryPrompt(s,"");

}

}

int main(int argc,char** argv){

 const bool smoke=argc==2 && std::strcmp(argv[1],"--smoke-test")==0;

 if(smoke)argc=1; // GTK must not interpret this test-only argument.

 AdwApplication* app=adw_application_new(kAppId,G_APPLICATION_DEFAULT_FLAGS);

 g_object_set_data(G_OBJECT(app),"studio-smoke",GINT_TO_POINTER(smoke));

 g_signal_connect(app,"startup",G_CALLBACK(+[](GApplication*,gpointer){app::loadStyle(kAppId);}),nullptr);

 g_signal_connect(app,"activate",G_CALLBACK(+[](GApplication* a,gpointer){

  buildWindow(GTK_APPLICATION(a));

  if(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(a),"studio-smoke"))){

   // Real GTK window must map under Xvfb/Wayland before success is reported.

   g_timeout_add(800,+[](gpointer p)->gboolean{

    auto* application=G_APPLICATION(p);

    GList* windows=gtk_application_get_windows(GTK_APPLICATION(application));

    const bool mapped=windows && gtk_widget_get_mapped(GTK_WIDGET(windows->data));

    gSmokeSucceeded=mapped;

    std::puts(mapped?"Wenuy Studio GTK smoke PASS: window mapped":"Wenuy Studio GTK smoke FAIL: window not mapped");

    std::fflush(stdout);

    g_application_quit(application);

    return G_SOURCE_REMOVE;

   },a);

  }

 }),nullptr);

 const int result=g_application_run(G_APPLICATION(app),argc,argv);

 g_object_unref(app);return smoke?(result==0&&gSmokeSucceeded?0:4):result;

}
