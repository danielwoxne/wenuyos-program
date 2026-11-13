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
#include "wenuystudio/PianoInput.hpp"
#include "wenuystudio/ProjectGuard.hpp"
#include "wenuystudio/InstrumentBank.hpp"
#include "wenuystudio/AudioImport.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>
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
 ws::ProjectGuard guard{};
 std::string file,recordFile;
 double playhead=0,recordAt=0;
 unsigned selected=0;
 std::optional<std::size_t> selectedNote;
 bool dirty=false;
 bool closeAllowed=false,chooserOpen=false;
 guint timer=0;
 ~Studio(){if(timer)g_source_remove(timer);playback.stop();recording.stop();}
};
struct Request{Studio* s;Pick pick;};
struct Indexed{Studio* s;std::size_t index;};
void refresh(Studio*);
void requestProtected(Studio*,ws::ProjectAction);
void performProtected(Studio*,const ws::ProjectAction&);
void save(Studio*);
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
 s->file.clear();s->history.clear();s->selected=0;s->selectedNote.reset();s->playhead=0;s->dirty=false;refresh(s);
}
void addTrack(Studio* s){
 if(s->project.tracks.size()>=128){notice(s,tr("Maximum 128 tracks","Högst 128 spår"));return;}
 checkpoint(s);ws::Track t;t.name=std::string(tr("Track ","Spår "))+std::to_string(s->project.tracks.size()+1);
 s->project.tracks.push_back(std::move(t));s->selected=static_cast<unsigned>(s->project.tracks.size()-1);s->selectedNote.reset();change(s);
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
void undo(Studio* s){if(s->history.undo(s->project)){s->selectedNote.reset();change(s);}}
void redo(Studio* s){if(s->history.redo(s->project)){s->selectedNote.reset();change(s);}}
void fileAction(Studio* s,Pick pick,const std::string& path){
 try{
  switch(pick){
   case Pick::Open:
    requestProtected(s,{ws::ProjectActionKind::Open,path});break;
   case Pick::Save:
    ws::saveProject(s->project,path);
    s->file=path;s->dirty=false;refresh(s);notice(s,tr("Project saved","Projekt sparat"));
    if(auto next=s->guard.finishedSave(true))performProtected(s,*next);
    break;
   case Pick::ImportWav:{
    if(s->project.tracks.size()>=128)throw std::runtime_error(tr("Maximum 128 tracks","Högst 128 spår"));
    const auto importDir=fs::u8path(g_get_user_data_dir())/"se.wenuyos.app.musikstudio"/"imports";
    const auto imported=ws::importAudioFile(path,importDir.u8string());
    const double len=double(imported.frames)/imported.rate;
    ws::Track t;t.name=fs::path(path).stem().string();
    t.clips.push_back({imported.filename,s->playhead,0,len,1});
    checkpoint(s);s->project.tracks.push_back(std::move(t));
    s->project.durationSeconds=std::min(20000.,std::max(s->project.durationSeconds,s->playhead+len));
    s->selected=static_cast<unsigned>(s->project.tracks.size()-1);
    s->selectedNote.reset();change(s);
    notice(s,imported.converted?tr("Audio imported with Datatypes","Ljud importerat med Datatyper"):
                    tr("WAV imported","WAV importerad"));break;}
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
 }catch(const std::exception& e){
  if(pick==Pick::Save){
   (void)s->guard.finishedSave(false);
   g_warning("Project save failed: %s",e.what());
   notice(s,tr("Could not save the project. Check disk space and permissions.",
               "Kunde inte spara projektet. Kontrollera ledigt utrymme och behörigheter."));
  }else if(pick==Pick::ImportWav){
   g_warning("Audio import failed: %s",e.what());
   notice(s,tr("Could not import audio. Check the format or enable WenuyOS Datatypes.",
               "Kunde inte importera ljud. Kontrollera formatet eller aktivera WenuyOS Datatyper."));
  }else notice(s,std::string(tr("Error: ","Fel: "))+e.what());
 }
}
void choose(Studio* s,Pick pick){
 if(s->chooserOpen){
  notice(s,tr("Finish the current file dialog first","Avsluta den pågående fildialogen först"));
  return;
 }
 s->chooserOpen=true;
 const bool save=pick==Pick::Save||pick==Pick::ExportWav||pick==Pick::ExportMidi||pick==Pick::Record;
 const char* title=pick==Pick::Open?tr("Open project","Öppna projekt"):
 pick==Pick::Save?tr("Save project","Spara projekt"):
 pick==Pick::ImportWav?tr("Import audio","Importera ljud"):
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
  Studio* st=req->s;
  st->chooserOpen=false;
  bool acted=false;
  if(response==GTK_RESPONSE_ACCEPT){
   GFile* file=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dlg));
   if(file){
    char* path=g_file_get_path(file);
    if(path){acted=true;fileAction(st,req->pick,path);g_free(path);}
    else notice(st,tr("Only local files are supported","Endast lokala filer stöds"));
    g_object_unref(file);
   }
  }
  // Cancelled Save As (or an inaccessible URI) never releases a pending New/Open/Close.
  if(!acted&&req->pick==Pick::Save)(void)st->guard.finishedSave(false);
  g_object_unref(dlg);
 }),req);
 gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}
void save(Studio* s){if(s->file.empty())choose(s,Pick::Save);else fileAction(s,Pick::Save,s->file);}

// Executes only AFTER the dirty guard allows the action, and only commits a
// loaded file after its full parse/validation succeeded.
void performProtected(Studio* s,const ws::ProjectAction& action){
 try{
  switch(action.kind){
   case ws::ProjectActionKind::New:newProject(s);break;
   case ws::ProjectActionKind::Open:{
    ws::Project loaded=ws::loadProject(action.path);
    stop(s);
    s->project=std::move(loaded);
    s->file=action.path;s->history.clear();s->selected=0;s->selectedNote.reset();
    s->playhead=0;s->dirty=false;refresh(s);
    notice(s,tr("Project opened","Projekt öppnat"));break;
   }
   case ws::ProjectActionKind::Close:
    s->closeAllowed=true;gtk_window_close(GTK_WINDOW(s->window));break;
  }
 }catch(const std::exception& e){
  g_warning("Project open failed: %s",e.what());
  notice(s,tr("Could not open the project. Check the file format and permissions.",
              "Kunde inte öppna projektet. Kontrollera filformat och behörigheter."));
 }
}
void requestProtected(Studio* s,ws::ProjectAction action){
 const bool blocked=s->recording.active()||!s->recordFile.empty()||s->chooserOpen;
 const auto result=s->guard.request(action,s->dirty,blocked);
 if(result==ws::GuardDecision::Busy){
  notice(s,blocked&&(s->recording.active()||!s->recordFile.empty())?
   tr("Stop recording before switching projects or closing",
      "Avsluta inspelningen innan du byter projekt eller stänger"):
   tr("Finish the current dialog first","Avsluta den pågående dialogen först"));
  return;
 }
 if(result==ws::GuardDecision::Proceed){performProtected(s,action);return;}
 auto* dialog=ADW_MESSAGE_DIALOG(adw_message_dialog_new(GTK_WINDOW(s->window),
  tr("Save changes?","Spara ändringarna?"),
  tr("Your music contains unsaved changes. What would you like to do?",
     "Musiken innehåller osparade ändringar. Vad vill du göra?")));
 adw_message_dialog_add_responses(dialog,
  "cancel",tr("Cancel","Avbryt"),
  "discard",tr("Discard changes","Kasta ändringar"),
  "save",tr("Save","Spara"),nullptr);
 adw_message_dialog_set_response_appearance(dialog,"discard",ADW_RESPONSE_DESTRUCTIVE);
 adw_message_dialog_set_default_response(dialog,"cancel");
 adw_message_dialog_set_close_response(dialog,"cancel");
 g_signal_connect(dialog,"response",G_CALLBACK(+[](AdwMessageDialog*,const char* response,gpointer user){
  auto* st=static_cast<Studio*>(user);
  if(g_strcmp0(response,"discard")==0){
   if(auto next=st->guard.discard())performProtected(st,*next);
  }else if(g_strcmp0(response,"save")==0){
   if(st->guard.beginSave())save(st);
  }else st->guard.cancel();
 }),s);
 gtk_window_present(GTK_WINDOW(dialog));
}
void record(Studio* s){if(s->recording.active())finishRecord(s);else choose(s,Pick::Record);}
GtkWidget* button(GtkWidget* box,Studio* s,const char* label,void(*callback)(Studio*)){
 GtkWidget* b=gtk_button_new_with_label(label);gtk_box_append(GTK_BOX(box),b);
 g_signal_connect_swapped(b,"clicked",G_CALLBACK(callback),s);return b;
}
void drawing(GtkDrawingArea* area,cairo_t* cr,int width,int height,gpointer user){
 auto* s=static_cast<Studio*>(user);const bool piano=GTK_WIDGET(area)==s->piano;
 GdkRGBA col{.5,.5,.5,1};gtk_style_context_get_color(gtk_widget_get_style_context(GTK_WIDGET(area)),&col);
 auto color=[&](double a){cairo_set_source_rgba(cr,col.red,col.green,col.blue,a);};
 const double scale=ws::pianoPixelsPerSecond,left=ws::pianoLeft,bar=4*60./s->project.bpm;
 for(double second=0;second<=s->project.durationSeconds+.01;second+=bar){
  const double x=left+second*scale;
  color(.18);cairo_move_to(cr,x,0);cairo_line_to(cr,x,height);cairo_stroke(cr);
  color(.75);cairo_set_font_size(cr,11);cairo_move_to(cr,x+3,15);
  cairo_show_text(cr,std::to_string(static_cast<int>(second/bar)+1).c_str());
 }
 if(piano){
  if(s->selected<s->project.tracks.size()){
   for(int key=ws::pianoLowestKey;key<=ws::pianoHighestKey;++key){
    const double y=ws::pianoTop+(ws::pianoHighestKey-key)*ws::pianoRowHeight;
    color(key%12==0?.25:.07);cairo_move_to(cr,0,y);cairo_line_to(cr,width,y);cairo_stroke(cr);
    if(key%12==0){color(.75);cairo_set_font_size(cr,11);cairo_move_to(cr,5,y+11);cairo_show_text(cr,ws::pitchName(key).c_str());}
   }
   const auto& notes=s->project.tracks[s->selected].notes;
   for(std::size_t i=0;i<notes.size();++i){
    const auto& n=notes[i];
    if(n.key<ws::pianoLowestKey||n.key>ws::pianoHighestKey)continue;
    const double x=left+n.atSeconds*scale;
    const double y=ws::pianoTop+(ws::pianoHighestKey-n.key)*ws::pianoRowHeight+1;
    const double w=std::max(4.,n.durationSeconds*scale);
    const bool active=s->selectedNote&&*s->selectedNote==i;
    color(active?1:.65);cairo_rectangle(cr,x,y,w,10);cairo_fill(cr);
    if(active){color(1);cairo_set_line_width(cr,1.5);cairo_rectangle(cr,x-.75,y-.75,w+1.5,11.5);cairo_stroke(cr);}
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
void deleteSelectedNote(Studio* s){
 if(!s->selectedNote||s->selected>=s->project.tracks.size())return;
 const auto index=*s->selectedNote;
 if(index>=s->project.tracks[s->selected].notes.size()){s->selectedNote.reset();return;}
 checkpoint(s);
 if(ws::removeNote(s->project,s->selected,index)){
  s->selectedNote.reset();change(s);notice(s,tr("Note deleted","Ton borttagen"));
 }
}
void moveSelectedNote(Studio* s,int pitchSteps,int timeSteps){
 if(!s->selectedNote||s->selected>=s->project.tracks.size())return;
 auto& notes=s->project.tracks[s->selected].notes;
 const auto index=*s->selectedNote;
 if(index>=notes.size()){s->selectedNote.reset();return;}
 try{
  const ws::MidiNote old=notes[index];
  const int key=std::clamp(old.key+pitchSteps,ws::pianoLowestKey,ws::pianoHighestKey);
  const ws::EditGrid grid{4,0};
  const double step=ws::beatSeconds(s->project.bpm)/grid.subdivisions;
  const double maximum=std::max(0.,s->project.durationSeconds-step*.01);
  const double at=ws::snapTime(std::clamp(old.atSeconds+timeSteps*step,0.,maximum),s->project.bpm,grid);
  if(at>=s->project.durationSeconds||(key==old.key&&std::abs(at-old.atSeconds)<1e-7))return;
  for(std::size_t i=0;i<notes.size();++i)
   if(i!=index&&notes[i].key==key&&std::abs(notes[i].atSeconds-at)<1e-7)return;
  checkpoint(s);
  if(!ws::moveNote(s->project,s->selected,index,key,at,grid))return;
  const auto& moved=s->project.tracks[s->selected].notes;
  for(std::size_t i=0;i<moved.size();++i)
   if(moved[i].key==key&&std::abs(moved[i].atSeconds-at)<1e-7){s->selectedNote=i;break;}
  s->playhead=at;change(s);
 }catch(const std::exception& e){notice(s,std::string(tr("Cannot move note: ","Kan inte flytta tonen: "))+e.what());}
}
void pressed(GtkGestureClick* gesture,int,double x,double y,gpointer data){
 auto* s=static_cast<Studio*>(data);
 s->playhead=std::clamp((x-ws::pianoLeft)/ws::pianoPixelsPerSecond,0.,s->project.durationSeconds);
 if(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture))==s->piano){
  gtk_widget_grab_focus(s->piano);
  if(s->selected<s->project.tracks.size()){
   const auto point=ws::pianoPositionAt(x,y,s->project.durationSeconds);
   if(point){
    const auto& track=s->project.tracks[s->selected];
    if(auto found=ws::pianoNoteAt(track,*point))s->selectedNote=*found;
    else if(auto found=ws::pianoNoteAtGrid(track,*point,s->project.bpm,{4,0}))s->selectedNote=*found;
    else try{
     checkpoint(s);
     const double length=ws::beatSeconds(s->project.bpm)/4.0;
     const auto result=ws::insertNote(s->project,s->selected,{point->key,point->seconds,length,.8f},{4,0});
     if(result.changed){s->selectedNote=result.index;change(s);}
    }catch(const std::exception& e){notice(s,std::string(tr("Cannot add note: ","Kan inte lägga till ton: "))+e.what());}
   }
  }
  gtk_widget_queue_draw(s->piano);
  gtk_widget_queue_draw(s->timeline);
  return;
 }
 if(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture))==s->timeline&&y>=28){
  const auto index=static_cast<std::size_t>((y-28)/66);
  if(index<s->project.tracks.size()&&index!=s->selected){
   s->selected=static_cast<unsigned>(index);s->selectedNote.reset();
  }
 }
 // Scrubbing must not recreate all GTK mixer/track controls on every click.
 gtk_widget_queue_draw(s->timeline);gtk_widget_queue_draw(s->piano);
}
void rightPressed(GtkGestureClick*,int,double x,double y,gpointer data){
 auto* s=static_cast<Studio*>(data);
 if(s->selected>=s->project.tracks.size())return;
 const auto point=ws::pianoPositionAt(x,y,s->project.durationSeconds);
 if(!point)return;
 if(auto found=ws::pianoNoteAt(s->project.tracks[s->selected],*point)){
  s->selectedNote=*found;deleteSelectedNote(s);
 }
}
GtkWidget* area(Studio* s,bool piano){
 GtkWidget* sc=gtk_scrolled_window_new();auto* draw=gtk_drawing_area_new();
 gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(draw),1024);
 gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(draw),piano?630:300);
 gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw),drawing,s,nullptr);
 GtkGesture* click=gtk_gesture_click_new();
 gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click),GDK_BUTTON_PRIMARY);
 g_signal_connect(click,"pressed",G_CALLBACK(pressed),s);
 gtk_widget_add_controller(draw,GTK_EVENT_CONTROLLER(click));
 if(piano){
  gtk_widget_set_focusable(draw,TRUE);
  gtk_accessible_update_property(GTK_ACCESSIBLE(draw),GTK_ACCESSIBLE_PROPERTY_LABEL,
   tr("Piano roll – click to add or select notes","Pianorulle – klicka för att lägga till eller välja toner"),-1);
  GtkGesture* secondary=gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(secondary),GDK_BUTTON_SECONDARY);
  g_signal_connect(secondary,"pressed",G_CALLBACK(rightPressed),s);
  gtk_widget_add_controller(draw,GTK_EVENT_CONTROLLER(secondary));
  GtkEventController* keys=gtk_event_controller_key_new();
  g_signal_connect(keys,"key-pressed",G_CALLBACK(+[](GtkEventControllerKey*,guint keyval,guint,
      GdkModifierType modifiers,gpointer data)->gboolean{
   if(modifiers&(GDK_CONTROL_MASK|GDK_ALT_MASK|GDK_SUPER_MASK))return FALSE;
   auto* st=static_cast<Studio*>(data);
   switch(keyval){
    case GDK_KEY_Delete:case GDK_KEY_BackSpace:
     if(!st->selectedNote)return FALSE;
     deleteSelectedNote(st);return TRUE;
    case GDK_KEY_Up:moveSelectedNote(st,1,0);return TRUE;
    case GDK_KEY_Down:moveSelectedNote(st,-1,0);return TRUE;
    case GDK_KEY_Left:moveSelectedNote(st,0,-1);return TRUE;
    case GDK_KEY_Right:moveSelectedNote(st,0,1);return TRUE;
    default:return FALSE;
   }
  }),s);
  gtk_widget_add_controller(draw,keys);
 }
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
  auto* instrumentLabel=gtk_label_new(tr("Instrument","Instrument"));
  gtk_label_set_xalign(GTK_LABEL(instrumentLabel),0);
  gtk_box_append(GTK_BOX(panel),instrumentLabel);
  std::vector<const char*> instrumentNames;
  instrumentNames.reserve(ws::instrumentCount()+1);
  for(std::size_t preset=0;preset<ws::instrumentCount();++preset){
   const auto* item=ws::instrumentById(static_cast<int>(preset));
   instrumentNames.push_back(sv()?item->swedish:item->english);
  }
  instrumentNames.push_back(nullptr);
  auto* inst=gtk_drop_down_new_from_strings(instrumentNames.data());
  gtk_widget_set_tooltip_text(inst,tr("Choose this track's synthesized instrument",
                                      "Välj syntetiserat instrument för spåret"));
  gtk_drop_down_set_selected(GTK_DROP_DOWN(inst),static_cast<guint>(t.instrument));
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
 g_signal_connect(s->window,"close-request",G_CALLBACK(+[](GtkWindow*,gpointer user)->gboolean{
  auto* st=static_cast<Studio*>(user);
  if(st->closeAllowed)return FALSE;
  if(!st->dirty&&!st->recording.active()&&st->recordFile.empty()&&!st->chooserOpen&&!st->guard.hasPending())return FALSE;
  requestProtected(st,{ws::ProjectActionKind::Close,{}});
  return TRUE;
 }),s);
 gtk_window_set_default_size(GTK_WINDOW(s->window),960,700);
 gtk_widget_set_size_request(s->window,690,480);
 auto* toolbar=adw_toolbar_view_new();
 auto* overlay=adw_toast_overlay_new();s->toast=overlay;
 auto* header=adw_header_bar_new();
 adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar),header);
 auto addTransport=[&](const char* icon,const char* caption,void(*fn)(Studio*)){
  auto* widget=gtk_button_new_from_icon_name(icon);
  gtk_widget_add_css_class(widget,"flat");
  gtk_widget_set_tooltip_text(widget,caption);
  gtk_accessible_update_property(GTK_ACCESSIBLE(widget),GTK_ACCESSIBLE_PROPERTY_LABEL,caption,-1);
  g_signal_connect_swapped(widget,"clicked",G_CALLBACK(fn),s);
  adw_header_bar_pack_end(ADW_HEADER_BAR(header),widget);
 };
 addTransport("media-record-symbolic",tr("Record","Spela in"),record);
 addTransport("media-playback-stop-symbolic",tr("Stop","Stoppa"),stop);
 addTransport("media-playback-start-symbolic",tr("Play","Spela"),play);
 auto* menu=gtk_menu_button_new();
 gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu),"open-menu-symbolic");
 gtk_widget_set_tooltip_text(menu,tr("Project and edit menu","Projekt och redigering"));
 gtk_accessible_update_property(GTK_ACCESSIBLE(menu),GTK_ACCESSIBLE_PROPERTY_LABEL,
  tr("Project and edit menu","Projekt och redigering"),-1);
 auto* model=g_menu_new();auto* files=g_menu_new();auto* edits=g_menu_new();
 auto addAction=[&](GMenu* section,const char* id,const char* caption){
  auto* act=g_simple_action_new(id,nullptr);
  g_signal_connect(act,"activate",G_CALLBACK(+[](GSimpleAction* a,GVariant*,gpointer user){
   auto* st=static_cast<Studio*>(user);const char* action=g_action_get_name(G_ACTION(a));
   if(g_strcmp0(action,"new")==0)requestProtected(st,{ws::ProjectActionKind::New,{}});
   else if(g_strcmp0(action,"open")==0)choose(st,Pick::Open);
   else if(g_strcmp0(action,"save")==0)save(st);
   else if(g_strcmp0(action,"import-wav")==0)choose(st,Pick::ImportWav);
   else if(g_strcmp0(action,"import-midi")==0)choose(st,Pick::ImportMidi);
   else if(g_strcmp0(action,"export-wav")==0)choose(st,Pick::ExportWav);
   else if(g_strcmp0(action,"export-midi")==0)choose(st,Pick::ExportMidi);
   else if(g_strcmp0(action,"undo")==0)undo(st);
   else if(g_strcmp0(action,"redo")==0)redo(st);
  }),s);
  g_action_map_add_action(G_ACTION_MAP(application),G_ACTION(act));g_object_unref(act);
  auto detailed=std::string("app.")+id;g_menu_append(section,caption,detailed.c_str());
 };
 addAction(files,"new",tr("New project","Nytt projekt"));
 addAction(files,"open",tr("Open project…","Öppna projekt…"));
 addAction(files,"save",tr("Save project","Spara projekt"));
 addAction(files,"import-wav",tr("Import audio…","Importera ljud…"));
 addAction(files,"import-midi",tr("Import MIDI…","Importera MIDI…"));
 addAction(files,"export-wav",tr("Export WAV…","Exportera WAV…"));
 addAction(files,"export-midi",tr("Export MIDI…","Exportera MIDI…"));
 addAction(edits,"undo",tr("Undo","Ångra"));
 addAction(edits,"redo",tr("Redo","Gör om"));
 g_menu_append_section(model,nullptr,G_MENU_MODEL(files));
 g_menu_append_section(model,nullptr,G_MENU_MODEL(edits));
 gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu),G_MENU_MODEL(model));
 g_object_unref(files);g_object_unref(edits);g_object_unref(model);
 adw_header_bar_pack_start(ADW_HEADER_BAR(header),menu);
 const char* openKeys[]={"<Control>o",nullptr};
 const char* saveKeys[]={"<Control>s",nullptr};
 const char* undoKeys[]={"<Control>z",nullptr};
 const char* redoKeys[]={"<Control><Shift>z",nullptr};
 gtk_application_set_accels_for_action(GTK_APPLICATION(application),"app.open",openKeys);
 gtk_application_set_accels_for_action(GTK_APPLICATION(application),"app.save",saveKeys);
 gtk_application_set_accels_for_action(GTK_APPLICATION(application),"app.undo",undoKeys);
 gtk_application_set_accels_for_action(GTK_APPLICATION(application),"app.redo",redoKeys);
 adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(overlay),toolbar);
 adw_application_window_set_content(ADW_APPLICATION_WINDOW(s->window),overlay);
 auto* main=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);

 auto* split=gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);gtk_widget_set_vexpand(split,TRUE);
 auto* trackScroll=gtk_scrolled_window_new();
 s->tracks=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(trackScroll),s->tracks);
 gtk_paned_set_start_child(GTK_PANED(split),trackScroll);
 auto* notebook=gtk_notebook_new();
 auto* arrangePage=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
 auto* arrangeBar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
 gtk_widget_set_margin_start(arrangeBar,8);gtk_widget_set_margin_top(arrangeBar,6);
 button(arrangeBar,s,tr("Add track","Lägg till spår"),addTrack);
 gtk_box_append(GTK_BOX(arrangePage),arrangeBar);
 auto* arrangeArea=area(s,false);gtk_widget_set_vexpand(arrangeArea,TRUE);
 gtk_box_append(GTK_BOX(arrangePage),arrangeArea);
 gtk_notebook_append_page(GTK_NOTEBOOK(notebook),arrangePage,gtk_label_new(tr("Arrangement","Arrangemang")));
 auto* pianoPage=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
 auto* pianoBar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
 gtk_widget_set_margin_start(pianoBar,8);gtk_widget_set_margin_top(pianoBar,6);
 button(pianoBar,s,tr("Add C4 note","Lägg till C4-ton"),addNote);
 button(pianoBar,s,tr("Quantize","Kvantisera"),quantize);
 gtk_box_append(GTK_BOX(pianoPage),pianoBar);
 auto* hint=gtk_label_new(tr("Click: add/select note · Right-click/Delete: remove · Arrows: move",
                          "Klick: lägg till/välj ton · Högerklick/Delete: ta bort · Pilar: flytta"));
 gtk_label_set_wrap(GTK_LABEL(hint),TRUE);
 gtk_label_set_xalign(GTK_LABEL(hint),0);
 gtk_widget_set_margin_start(hint,10);gtk_widget_set_margin_end(hint,10);
 gtk_widget_add_css_class(hint,"dim-label");
 gtk_box_append(GTK_BOX(pianoPage),hint);
 auto* pianoArea=area(s,true);gtk_widget_set_vexpand(pianoArea,TRUE);
 gtk_box_append(GTK_BOX(pianoPage),pianoArea);
 gtk_notebook_append_page(GTK_NOTEBOOK(notebook),pianoPage,gtk_label_new(tr("Piano roll","Pianorulle")));
 auto* mixerScroll=gtk_scrolled_window_new();s->mixer=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(mixerScroll),s->mixer);
 gtk_notebook_append_page(GTK_NOTEBOOK(notebook),mixerScroll,gtk_label_new(tr("Mixer","Mixer")));
 // Compact, discoverable catalogue: changing instrument affects only the selected track.
 auto* instrumentScroll=gtk_scrolled_window_new();
 auto* library=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
 gtk_widget_set_margin_start(library,12);gtk_widget_set_margin_end(library,12);
 gtk_widget_set_margin_top(library,10);gtk_widget_set_margin_bottom(library,12);
 auto* libraryHint=gtk_label_new(tr(
  "Choose an instrument for the selected track. Sounds are synthesized and work offline.",
  "Välj instrument för markerat spår. Ljuden skapas med syntes och fungerar offline."));
 gtk_label_set_wrap(GTK_LABEL(libraryHint),TRUE);
 gtk_label_set_xalign(GTK_LABEL(libraryHint),0);
 gtk_box_append(GTK_BOX(library),libraryHint);
 const char* families[]={"Synthesizers","Keyboards","Strings","Winds","Percussion","Voices"};
 for(const char* family:families){
  const ws::InstrumentPreset* first=nullptr;
  for(std::size_t i=0;i<ws::instrumentCount();++i){
   const auto* preset=ws::instrumentById(static_cast<int>(i));
   if(std::strcmp(preset->familyEnglish,family)==0){first=preset;break;}
  }
  if(!first)continue;
  auto* groupTitle=gtk_label_new(sv()?first->familySwedish:first->familyEnglish);
  gtk_widget_set_halign(groupTitle,GTK_ALIGN_START);
  gtk_widget_add_css_class(groupTitle,"heading");
  gtk_box_append(GTK_BOX(library),groupTitle);
  auto* wrap=gtk_flow_box_new();
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(wrap),GTK_SELECTION_NONE);
  gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(wrap),3);
  gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(wrap),6);
  gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(wrap),6);
  for(std::size_t i=0;i<ws::instrumentCount();++i){
   const auto* preset=ws::instrumentById(static_cast<int>(i));
   if(std::strcmp(preset->familyEnglish,family)!=0)continue;
   auto* presetButton=gtk_button_new_with_label(sv()?preset->swedish:preset->english);
   auto* binding=index(presetButton,s,i);
   g_signal_connect(presetButton,"clicked",G_CALLBACK(+[](GtkButton*,gpointer user){
    auto* choice=static_cast<Indexed*>(user);
    auto* st=choice->s;
    if(st->selected>=st->project.tracks.size())return;
    if(st->project.tracks[st->selected].instrument==static_cast<int>(choice->index))return;
    checkpoint(st);
    st->project.tracks[st->selected].instrument=static_cast<int>(choice->index);
    change(st);
    notice(st,tr("Instrument applied to selected track","Instrument valt för markerat spår"));
   }),binding);
   gtk_flow_box_insert(GTK_FLOW_BOX(wrap),presetButton,-1);
  }
  gtk_box_append(GTK_BOX(library),wrap);
 }
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(instrumentScroll),library);
 gtk_notebook_append_page(GTK_NOTEBOOK(notebook),instrumentScroll,
                          gtk_label_new(tr("Instrument library","Instrumentbibliotek")));
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
void openProjectFiles(GApplication* app,GFile** files,gint nfiles,const char*,gpointer){
 if(nfiles<1)return;
 if(!gtk_application_get_active_window(GTK_APPLICATION(app)))activate(app,nullptr);
 auto* st=static_cast<Studio*>(g_object_get_data(G_OBJECT(app),"studio"));
 if(!st)return;
 char* filename=g_file_get_path(files[0]);
 if(!filename){notice(st,tr("Only local project files are supported","Endast lokala projektfiler stöds"));return;}
 fileAction(st,Pick::Open,filename);g_free(filename);
 gtk_window_present(GTK_WINDOW(st->window));
}
int main(int argc,char** argv){
 bool smoke=false;
 for(int i=1;i<argc;++i)if(std::strcmp(argv[i],"--smoke-test")==0){
  smoke=true;for(int j=i;j<argc-1;++j)argv[j]=argv[j+1];--argc;break;
 }
 AdwApplication* app=adw_application_new("se.wenuyos.app.musikstudio",G_APPLICATION_HANDLES_OPEN);
 g_signal_connect(app,"activate",G_CALLBACK(activate),nullptr);
 g_signal_connect(app,"open",G_CALLBACK(openProjectFiles),nullptr);
 if(smoke)g_timeout_add(1000,+[](gpointer data)->gboolean{
  auto* win=gtk_application_get_active_window(GTK_APPLICATION(data));
  std::puts(win&&gtk_widget_get_mapped(GTK_WIDGET(win))?"Wenuy Studio GTK smoke PASS":"Wenuy Studio GTK smoke FAIL");
  g_application_quit(G_APPLICATION(data));return G_SOURCE_REMOVE;
 },app);
 const int result=g_application_run(G_APPLICATION(app),argc,argv);
 g_object_unref(app);return result;
}
