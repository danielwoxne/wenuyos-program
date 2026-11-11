// SPDX-License-Identifier: GPL-3.0-or-later
#include <adwaita.h>
#include "wenuystudio/Engine.hpp"
#include "wenuystudio/Playback.hpp"
#include "wenuystudio/ProjectIO.hpp"
#include "wenuystudio/Sequencer.hpp"
#include "wenuystudio/EditModel.hpp"
#include "wenuystudio/Arrangement.hpp"
#include "wenuystudio/Wave.hpp"
#include "wenuystudio/MidiFile.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
namespace ws=wenuystudio;
namespace {
bool sv(){const char* s=std::getenv("LANGUAGE");if(!s||!*s)s=std::getenv("LANG");return s&&std::strncmp(s,"sv",2)==0;}
const char* tr(const char* en,const char* se){return sv()?se:en;}
enum class Pick {Open,Save,ImportWav,ExportWav,ImportMidi,ExportMidi,Record};
struct Studio{
 GtkWidget *window=nullptr,*toast=nullptr,*status=nullptr,*tracks=nullptr,*timeline=nullptr,*piano=nullptr,*mixer=nullptr;
 ws::Project project{};
 ws::Playback playback{};
 ws::Recording recording{};
 ws::EditHistory history{};
 std::string file,recordFile;
 double playhead=0,recordAt=0;
 unsigned selected=0;
 bool dirty=false;
 guint timer=0;
 ~Studio(){if(timer)g_source_remove(timer);playback.stop();recording.stop();}
};
struct Request{Studio* s;Pick pick;};
struct Indexed{Studio* s;std::size_t index;};
void refresh(Studio*);
void notice(Studio* s,const std::string& text){
 if(s->status)gtk_label_set_text(GTK_LABEL(s->status),text.c_str());
 if(s->toast)adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(s->toast),adw_toast_new(text.c_str()));
}
void change(Studio* s){s->dirty=true;refresh(s);}
void checkpoint(Studio* s){s->history.checkpoint(s->project);}
void finishRecord(Studio* s){
 if(s->recordFile.empty())return;
 s->recording.stop();
 try{
  if(!s->recording.completed())throw std::runtime_error(s->recording.error());
  ws::WaveReader r(s->recordFile);
  if(s->project.tracks.size()>=128)throw std::runtime_error("Too many tracks");
  ws::Track t;t.name=tr("Microphone","Mikrofon");
  const double d=double(r.frames())/r.rate();
  t.clips.push_back({fs::absolute(s->recordFile).string(),s->recordAt,0,d,1});
  checkpoint(s);s->project.tracks.push_back(std::move(t));
  s->project.durationSeconds=std::min(20000.,std::max(s->project.durationSeconds,s->recordAt+d));
  s->selected=static_cast<unsigned>(s->project.tracks.size()-1);
  change(s);notice(s,tr("Recording saved","Inspelningen sparad"));
 }catch(const std::exception& e){notice(s,std::string(tr("Recording failed: ","Inspelningen misslyckades: "))+e.what());}
 s->recordFile.clear();
}
gboolean tick(gpointer data){
 auto* s=static_cast<Studio*>(data);
 if(s->playback.active()){s->playhead=std::min(s->project.durationSeconds,s->playback.position());
  gtk_widget_queue_draw(s->timeline);gtk_widget_queue_draw(s->piano);}
 if(!s->recordFile.empty()&&!s->recording.active())finishRecord(s);
 if(!s->playback.active()&&!s->recording.active()){
  if(!s->playback.error().empty())notice(s,std::string(tr("Audio error: ","Ljudfel: "))+s->playback.error());
  s->timer=0;return G_SOURCE_REMOVE;
 }
 return G_SOURCE_CONTINUE;
}
void startTick(Studio* s){if(!s->timer)s->timer=g_timeout_add(150,tick,s);}
void play(Studio* s){
 try{if(!s->recordFile.empty())finishRecord(s);s->playback.start(s->project,std::min(s->playhead,std::max(0.,s->project.durationSeconds-.01)));
 startTick(s);notice(s,tr("Playing","Spelar"));}catch(const std::exception& e){notice(s,e.what());}
}
void stop(Studio* s){s->playback.stop();if(!s->recordFile.empty())finishRecord(s);notice(s,tr("Stopped","Stoppad"));}
void newProject(Studio* s){
 stop(s);s->project=ws::Project{};ws::Track t;t.name=tr("Track 1","Spår 1");s->project.tracks.push_back(std::move(t));
 s->file.clear();s->history.clear();s->selected=0;s->playhead=0;s->dirty=false;refresh(s);
}
void addTrack(Studio* s){
 if(s->project.tracks.size()>=128){notice(s,tr("Maximum 128 tracks","Högst 128 spår"));return;}
 checkpoint(s);ws::Track t;t.name=std::string(tr("Track ","Spår "))+std::to_string(s->project.tracks.size()+1);
 s->project.tracks.push_back(std::move(t));s->selected=static_cast<unsigned>(s->project.tracks.size()-1);change(s);
}
void addNote(Studio* s){
 if(s->selected>=s->project.tracks.size())return;
 try{checkpoint(s);ws::insertNote(s->project,s->selected,{60,s->playhead,.5,.8f},{4,0});change(s);}
 catch(const std::exception& e){notice(s,e.what());}
}
void quantize(Studio* s){
 if(s->selected>=s->project.tracks.size())return;
 try{checkpoint(s);if(ws::quantizeTrack(s->project,s->selected,{4,0}))change(s);}
 catch(const std::exception& e){notice(s,e.what());}
}
void undo(Studio* s){if(s->history.undo(s->project))change(s);}
void redo(Studio* s){if(s->history.redo(s->project))change(s);}
void fileAction(Studio* s,Pick pick,const std::string& path){
 try{
  switch(pick){
   case Pick::Open:
    stop(s);s->project=ws::loadProject(path);s->file=path;s->history.clear();s->selected=0;s->playhead=0;s->dirty=false;refresh(s);
    notice(s,tr("Project opened","Projekt öppnat"));break;
   case Pick::Save:
    ws::saveProject(s->project,path);s->file=path;s->dirty=false;refresh(s);notice(s,tr("Project saved","Projekt sparat"));break;
   case Pick::ImportWav:{
    if(s->project.tracks.size()>=128)throw std::runtime_error("Maximum 128 tracks");
    ws::WaveReader r(path);double len=double(r.frames())/r.rate();
    ws::Track t;t.name=fs::path(path).stem().string();t.clips.push_back({fs::absolute(path).string(),s->playhead,0,len,1});
    checkpoint(s);s->project.tracks.push_back(std::move(t));
    s->project.durationSeconds=std::min(20000.,std::max(s->project.durationSeconds,s->playhead+len));
    s->selected=static_cast<unsigned>(s->project.tracks.size()-1);change(s);notice(s,tr("WAV imported","WAV importerad"));break;}
   case Pick::ImportMidi:{
    auto project=ws::importMidi(path);
    if(s->project.tracks.size()+project.tracks.size()>128)throw std::runtime_error("Maximum 128 tracks");
    checkpoint(s);s->project.durationSeconds=std::min(20000.,std::max(s->project.durationSeconds,project.durationSeconds));
    for(auto& t:project.tracks){
     s->project.tracks.push_back(std::move(t));
    }
    change(s);
    notice(s,tr("MIDI imported","MIDI importerad"));break;}
   case Pick::ExportWav:
    if(fs::exists(path))throw std::runtime_error(tr("File already exists","Filen finns redan"));
    ws::render(s->project,path);notice(s,tr("WAV exported","WAV exporterad"));break;
   case Pick::ExportMidi:
    if(fs::exists(path))throw std::runtime_error(tr("File already exists","Filen finns redan"));
    ws::exportMidi(s->project,path);notice(s,tr("MIDI exported","MIDI exporterad"));break;
   case Pick::Record:
    if(fs::exists(path))throw std::runtime_error(tr("File already exists","Filen finns redan"));
    s->playback.stop();s->recordAt=s->playhead;s->recordFile=path;s->recording.start(path,s->project.sampleRate);
    startTick(s);notice(s,tr("Recording – press Stop to finish","Spelar in – tryck Stoppa för att avsluta"));break;
  }
 }catch(const std::exception& e){notice(s,std::string(tr("Error: ","Fel: "))+e.what());}
}
void choose(Studio* s,Pick pick){
 const bool save=pick==Pick::Save||pick==Pick::ExportWav||pick==Pick::ExportMidi||pick==Pick::Record;
 const char* title=pick==Pick::Open?tr("Open project","Öppna projekt"):
 pick==Pick::Save?tr("Save project","Spara projekt"):
 pick==Pick::ImportWav?tr("Import WAV","Importera WAV"):
 pick==Pick::ExportWav?tr("Export WAV","Exportera WAV"):
 pick==Pick::ImportMidi?tr("Import MIDI","Importera MIDI"):
 pick==Pick::ExportMidi?tr("Export MIDI","Exportera MIDI"):tr("Record WAV","Spela in WAV");
 GtkFileChooserNative* dialog=gtk_file_chooser_native_new(title,GTK_WINDOW(s->window),
 save?GTK_FILE_CHOOSER_ACTION_SAVE:GTK_FILE_CHOOSER_ACTION_OPEN,
 save?tr("Save","Spara"):tr("Open","Öppna"),tr("Cancel","Avbryt"));
 if(save){gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
 pick==Pick::Save?"song.wstudio":pick==Pick::ExportMidi?"song.mid":pick==Pick::Record?"recording.wav":"mix.wav");}
 auto* req=new Request{s,pick};
 g_signal_connect(dialog,"response",G_CALLBACK(+[](GtkNativeDialog* dlg,int response,gpointer data){
  std::unique_ptr<Request> req(static_cast<Request*>(data));
  if(response==GTK_RESPONSE_ACCEPT){
   GFile* file=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dlg));
   if(file){char* path=g_file_get_path(file);if(path){fileAction(req->s,req->pick,path);g_free(path);}g_object_unref(file);}
  }
  g_object_unref(dlg);
 }),req);
 gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}
void save(Studio* s){if(s->file.empty())choose(s,Pick::Save);else fileAction(s,Pick::Save,s->file);}
void record(Studio* s){if(s->recording.active())finishRecord(s);else choose(s,Pick::Record);}
GtkWidget* button(GtkWidget* box,Studio* s,const char* label,void(*callback)(Studio*)){
 GtkWidget* b=gtk_button_new_with_label(label);gtk_box_append(GTK_BOX(box),b);
 g_signal_connect_swapped(b,"clicked",G_CALLBACK(callback),s);return b;
}
void drawing(GtkDrawingArea* area,cairo_t* cr,int width,int height,gpointer user){
 auto* s=static_cast<Studio*>(user);const bool piano=GTK_WIDGET(area)==s->piano;
 GdkRGBA col{.5,.5,.5,1};gtk_style_context_get_color(gtk_widget_get_style_context(GTK_WIDGET(area)),&col);
 auto color=[&](double a){cairo_set_source_rgba(cr,col.red,col.green,col.blue,a);};
 const double scale=50,left=65,bar=4*60./s->project.bpm;
 for(double second=0;second<=s->project.durationSeconds+.01;second+=bar){
  const double x=left+second*scale;
  color(.18);cairo_move_to(cr,x,0);cairo_line_to(cr,x,height);cairo_stroke(cr);
  color(.75);cairo_set_font_size(cr,11);cairo_move_to(cr,x+3,15);
  cairo_show_text(cr,std::to_string(static_cast<int>(second/bar)+1).c_str());
 }
 if(piano){
  if(s->selected<s->project.tracks.size()){
   for(int key=36;key<=84;++key){
    const double y=28+(84-key)*12;color(key%12==0?.25:.07);cairo_move_to(cr,0,y);cairo_line_to(cr,width,y);cairo_stroke(cr);
    if(key%12==0){color(.75);cairo_set_font_size(cr,11);cairo_move_to(cr,5,y+11);cairo_show_text(cr,ws::pitchName(key).c_str());}
   }
   for(const auto& n:s->project.tracks[s->selected].notes){
    if(n.key<36||n.key>84)continue;
    color(.72);cairo_rectangle(cr,left+n.atSeconds*scale,28+(84-n.key)*12+1,std::max(4.,n.durationSeconds*scale),10);cairo_fill(cr);
   }
  }
 }else for(std::size_t i=0;i<s->project.tracks.size();++i){
  const double y=28+i*66;color(.15);cairo_move_to(cr,0,y+64);cairo_line_to(cr,width,y+64);cairo_stroke(cr);
  color(i==s->selected?1:.55);cairo_set_font_size(cr,12);cairo_move_to(cr,5,y+20);cairo_show_text(cr,s->project.tracks[i].name.c_str());
  for(const auto& c:s->project.tracks[i].clips){color(.20);cairo_rectangle(cr,left+c.startSeconds*scale,y+8,std::max(5.,c.durationSeconds*scale),46);cairo_fill(cr);}
  for(const auto& n:s->project.tracks[i].notes){color(.7);cairo_rectangle(cr,left+n.atSeconds*scale,y+22,std::max(4.,n.durationSeconds*scale),14);cairo_fill(cr);}
 }
 color(.9);cairo_move_to(cr,left+s->playhead*scale,0);cairo_line_to(cr,left+s->playhead*scale,height);cairo_stroke(cr);
}
void pressed(GtkGestureClick* gesture,int,double x,double y,gpointer data){
 auto* s=static_cast<Studio*>(data);
 s->playhead=std::clamp((x-65)/50.,0.,s->project.durationSeconds);
 if(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture))==s->timeline && y>=28){
  auto index=static_cast<std::size_t>((y-28)/66);
  if(index<s->project.tracks.size())s->selected=static_cast<unsigned>(index);
 }
 refresh(s);
}
GtkWidget* area(Studio* s,bool piano){
 GtkWidget* sc=gtk_scrolled_window_new();auto* draw=gtk_drawing_area_new();
 gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(draw),1024);
 gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(draw),piano?630:300);
 gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw),drawing,s,nullptr);
 GtkGesture* click=gtk_gesture_click_new();
 g_signal_connect(click,"pressed",G_CALLBACK(pressed),s);
 gtk_widget_add_controller(draw,GTK_EVENT_CONTROLLER(click));
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sc),draw);
 if(piano)s->piano=draw;else s->timeline=draw;
 return sc;
}
Indexed* index(GtkWidget* w,Studio* s,std::size_t i){
 auto* c=new Indexed{s,i};g_object_set_data_full(G_OBJECT(w),"index",c,[](gpointer data){delete static_cast<Indexed*>(data);});return c;
}
void refresh(Studio* s){
 if(!s->window)return;
 while(auto* child=gtk_widget_get_first_child(s->tracks))gtk_box_remove(GTK_BOX(s->tracks),child);
 while(auto* child=gtk_widget_get_first_child(s->mixer))gtk_box_remove(GTK_BOX(s->mixer),child);
 for(std::size_t i=0;i<s->project.tracks.size();++i){
  const auto& t=s->project.tracks[i];
  auto* panel=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
  gtk_widget_set_margin_start(panel,9);gtk_widget_set_margin_top(panel,8);
  gtk_widget_set_margin_end(panel,9);gtk_widget_set_margin_bottom(panel,8);
  auto* name=gtk_label_new(t.name.c_str());gtk_label_set_xalign(GTK_LABEL(name),0);
  gtk_widget_add_css_class(name,"heading");gtk_box_append(GTK_BOX(panel),name);
  auto* mute=gtk_check_button_new_with_label(tr("Mute","Tyst"));
  gtk_check_button_set_active(GTK_CHECK_BUTTON(mute),t.mute);
  auto* c=index(mute,s,i);
  g_signal_connect(mute,"toggled",G_CALLBACK(+[](GtkCheckButton* w,gpointer data){
   auto* c=static_cast<Indexed*>(data);c->s->project.tracks[c->index].mute=gtk_check_button_get_active(w);c->s->dirty=true;
  }),c);
  gtk_box_append(GTK_BOX(panel),mute);
  auto* solo=gtk_check_button_new_with_label(tr("Solo","Solo"));
  gtk_check_button_set_active(GTK_CHECK_BUTTON(solo),t.solo);
  c=index(solo,s,i);
  g_signal_connect(solo,"toggled",G_CALLBACK(+[](GtkCheckButton* w,gpointer data){
   auto* c=static_cast<Indexed*>(data);c->s->project.tracks[c->index].solo=gtk_check_button_get_active(w);c->s->dirty=true;
  }),c);
  gtk_box_append(GTK_BOX(panel),solo);
  auto* volume=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,2,.05);
  gtk_range_set_value(GTK_RANGE(volume),t.gain);
  gtk_widget_set_tooltip_text(volume,tr("Volume","Volym"));
  c=index(volume,s,i);
  g_signal_connect(volume,"value-changed",G_CALLBACK(+[](GtkRange* w,gpointer data){
   auto* c=static_cast<Indexed*>(data);c->s->project.tracks[c->index].gain=static_cast<float>(gtk_range_get_value(w));c->s->dirty=true;
  }),c);
  gtk_box_append(GTK_BOX(panel),volume);
  static const char* instrumentSV[]={"Sinus","Triangel","Orgel","Varm synt",nullptr};
  static const char* instrumentEN[]={"Sine","Triangle","Organ","Warm synth",nullptr};
  auto* inst=gtk_drop_down_new_from_strings(sv()?instrumentSV:instrumentEN);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(inst),std::clamp(t.instrument,0,3));
  c=index(inst,s,i);
  g_signal_connect(inst,"notify::selected",G_CALLBACK(+[](GtkDropDown* w,GParamSpec*,gpointer data){
   auto* c=static_cast<Indexed*>(data);c->s->project.tracks[c->index].instrument=static_cast<int>(gtk_drop_down_get_selected(w));c->s->dirty=true;
  }),c);
  gtk_box_append(GTK_BOX(panel),inst);gtk_box_append(GTK_BOX(s->tracks),panel);
  auto* mix=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
  gtk_widget_set_margin_start(mix,12);gtk_widget_set_margin_top(mix,10);
  gtk_widget_set_margin_end(mix,12);gtk_widget_set_margin_bottom(mix,10);
  gtk_box_append(GTK_BOX(mix),gtk_label_new(t.name.c_str()));
  auto* fx=gtk_check_button_new_with_label(tr("Enable EQ/compressor","Aktivera EQ/kompressor"));
  gtk_check_button_set_active(GTK_CHECK_BUTTON(fx),t.effects.enabled);
  c=index(fx,s,i);
  g_signal_connect(fx,"toggled",G_CALLBACK(+[](GtkCheckButton* w,gpointer data){
   auto* c=static_cast<Indexed*>(data);c->s->project.tracks[c->index].effects.enabled=gtk_check_button_get_active(w);c->s->dirty=true;
  }),c);
  gtk_box_append(GTK_BOX(mix),fx);
  for(int k=0;k<3;++k){
   auto* row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,7);
   gtk_box_append(GTK_BOX(row),gtk_label_new(k==0?tr("Bass","Bas"):k==1?tr("Mid","Mellan"):tr("Treble","Diskant")));
   auto* scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-18,18,1);
   gtk_widget_set_hexpand(scale,TRUE);
   gtk_range_set_value(GTK_RANGE(scale),k==0?t.effects.lowDb:k==1?t.effects.midDb:t.effects.highDb);
   c=index(scale,s,i);g_object_set_data(G_OBJECT(scale),"band",GINT_TO_POINTER(k));
   g_signal_connect(scale,"value-changed",G_CALLBACK(+[](GtkRange* w,gpointer data){
    auto* c=static_cast<Indexed*>(data);auto& fx=c->s->project.tracks[c->index].effects;
    float v=static_cast<float>(gtk_range_get_value(w));
    switch(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"band"))){case 0:fx.lowDb=v;break;case 1:fx.midDb=v;break;default:fx.highDb=v;}
    c->s->dirty=true;
   }),c);
   gtk_box_append(GTK_BOX(row),scale);gtk_box_append(GTK_BOX(mix),row);
  }
  gtk_box_append(GTK_BOX(s->mixer),mix);
 }
 gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(s->timeline),static_cast<int>(std::clamp(s->project.durationSeconds*50+80,950.,20000.)));
 gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(s->timeline),std::max(200,static_cast<int>(s->project.tracks.size()*66+45)));
 gtk_widget_queue_draw(s->timeline);gtk_widget_queue_draw(s->piano);
 gtk_window_set_title(GTK_WINDOW(s->window),(std::string("Wenuy Studio – ")+(s->file.empty()?tr("Untitled","Namnlöst"):fs::path(s->file).filename().string())+(s->dirty?" *":"")).c_str());
}
void activate(GApplication* application,gpointer){
 if(auto* open=gtk_application_get_active_window(GTK_APPLICATION(application))){gtk_window_present(open);return;}
 auto* s=new Studio();
 g_object_set_data_full(G_OBJECT(application),"studio",s,[](gpointer p){delete static_cast<Studio*>(p);});
 s->window=adw_application_window_new(GTK_APPLICATION(application));
 gtk_window_set_default_size(GTK_WINDOW(s->window),1150,760);
 auto* toolbar=adw_toolbar_view_new();
 auto* overlay=adw_toast_overlay_new();s->toast=overlay;
 adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar),adw_header_bar_new());
 adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(overlay),toolbar);
 adw_application_window_set_content(ADW_APPLICATION_WINDOW(s->window),overlay);
 auto* main=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
 auto* bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,5);
 gtk_widget_set_margin_start(bar,10);gtk_widget_set_margin_end(bar,10);
 button(bar,s,tr("New","Nytt"),newProject);
 button(bar,s,tr("Open","Öppna"),+[](Studio* st){choose(st,Pick::Open);});
 button(bar,s,tr("Save","Spara"),save);
 button(bar,s,tr("WAV import","Importera WAV"),+[](Studio* st){choose(st,Pick::ImportWav);});
 button(bar,s,tr("MIDI import","Importera MIDI"),+[](Studio* st){choose(st,Pick::ImportMidi);});
 button(bar,s,tr("Export WAV","Exportera WAV"),+[](Studio* st){choose(st,Pick::ExportWav);});
 button(bar,s,tr("Export MIDI","Exportera MIDI"),+[](Studio* st){choose(st,Pick::ExportMidi);});
 button(bar,s,tr("Play","Spela"),play);
 button(bar,s,tr("Stop","Stoppa"),stop);
 button(bar,s,tr("Record","Spela in"),record);
 button(bar,s,tr("+Track","+Spår"),addTrack);
 button(bar,s,tr("+Note","+Not"),addNote);
 button(bar,s,tr("Quantize","Kvantisera"),quantize);
 button(bar,s,tr("Undo","Ångra"),undo);
 button(bar,s,tr("Redo","Gör om"),redo);
 auto* buttonScroll=gtk_scrolled_window_new();
 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(buttonScroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_NEVER);
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(buttonScroll),bar);
 gtk_box_append(GTK_BOX(main),buttonScroll);
 auto* split=gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);gtk_widget_set_vexpand(split,TRUE);
 auto* trackScroll=gtk_scrolled_window_new();
 s->tracks=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(trackScroll),s->tracks);
 gtk_paned_set_start_child(GTK_PANED(split),trackScroll);
 auto* notebook=gtk_notebook_new();
 gtk_notebook_append_page(GTK_NOTEBOOK(notebook),area(s,false),gtk_label_new(tr("Arrangement","Arrangemang")));
 gtk_notebook_append_page(GTK_NOTEBOOK(notebook),area(s,true),gtk_label_new(tr("Piano roll","Pianorulle")));
 auto* mixerScroll=gtk_scrolled_window_new();s->mixer=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(mixerScroll),s->mixer);
 gtk_notebook_append_page(GTK_NOTEBOOK(notebook),mixerScroll,gtk_label_new(tr("Mixer","Mixer")));
 gtk_paned_set_end_child(GTK_PANED(split),notebook);gtk_paned_set_position(GTK_PANED(split),235);
 gtk_box_append(GTK_BOX(main),split);
 s->status=gtk_label_new(tr("Ready – open a project or create music","Redo – öppna ett projekt eller skapa musik"));
 gtk_label_set_xalign(GTK_LABEL(s->status),0);gtk_widget_set_margin_start(s->status,10);
 gtk_widget_set_margin_bottom(s->status,8);gtk_box_append(GTK_BOX(main),s->status);
 adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar),main);
 ws::Track track;track.name=tr("Track 1","Spår 1");s->project.tracks.push_back(std::move(track));
 refresh(s);gtk_window_present(GTK_WINDOW(s->window));
}
}
int main(int argc,char** argv){
 const bool smoke=argc==2&&std::strcmp(argv[1],"--smoke-test")==0;
 if(smoke)argc=1;
 AdwApplication* app=adw_application_new("se.wenuyos.app.musikstudio",G_APPLICATION_DEFAULT_FLAGS);
 g_signal_connect(app,"activate",G_CALLBACK(activate),nullptr);
 if(smoke)g_timeout_add(1000,+[](gpointer data)->gboolean{
  auto* win=gtk_application_get_active_window(GTK_APPLICATION(data));
  std::puts(win&&gtk_widget_get_mapped(GTK_WIDGET(win))?"Wenuy Studio GTK smoke PASS":"Wenuy Studio GTK smoke FAIL");
  g_application_quit(G_APPLICATION(data));return G_SOURCE_REMOVE;
 },app);
 const int result=g_application_run(G_APPLICATION(app),argc,argv);
 g_object_unref(app);return result;
}
