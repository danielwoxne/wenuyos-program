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
#include "wenuystudio/KeyboardInput.hpp"
#include "wenuystudio/MidiTake.hpp"
#include "wenuystudio/MidiInput.hpp"
#include "wenuystudio/AudioImport.hpp"
#include <algorithm>
#include <array>
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
enum class Pick {Open,Save,ImportWav,ExportWav,ImportMidi,ExportMidi,ExportGfx,Record};
struct Studio{
 GtkWidget *window=nullptr,*toast=nullptr,*status=nullptr,*tracks=nullptr,*timeline=nullptr,*piano=nullptr,*mixer=nullptr;
 GtkWidget *keyboard=nullptr,*midiButton=nullptr,*keyboardStatus=nullptr;
 GtkWidget *midiDevicePicker=nullptr,*midiDeviceButton=nullptr,*midiDeviceStatus=nullptr;
 GtkStringList* midiDeviceNames=nullptr;
 ws::Project project{};
 ws::Playback playback{};
 ws::Playback audition{};
 ws::Recording recording{};
 ws::MidiTake midi{};
 ws::MidiInput midiInput{};
 std::vector<ws::MidiInputSource> midiSources{};
 std::array<bool,128> activeKeys{},externalKeys{};
 int pointerKey=-1;
 unsigned midiTrack=0;
 bool previewUnavailable=false;
 ws::EditHistory history{};
 ws::ProjectGuard guard{};
 std::string file,recordFile;
 double playhead=0,recordAt=0;
 unsigned selected=0;
 std::optional<std::size_t> selectedNote;
 bool dirty=false;
 bool closeAllowed=false,chooserOpen=false;
 guint timer=0;
 ~Studio(){
  midiInput.disconnect();
  if(timer)g_source_remove(timer);
  audition.stop();playback.stop();recording.stop();
 }
};
struct Request{Studio* s;Pick pick;};
struct MapRequest{
 GWeakRef overlay{};
 explicit MapRequest(GtkWidget* widget){g_weak_ref_init(&overlay,G_OBJECT(widget));}
 ~MapRequest(){g_weak_ref_clear(&overlay);}
};
struct Indexed{Studio* s;std::size_t index;};
void refresh(Studio*);
void requestProtected(Studio*,ws::ProjectAction);
void performProtected(Studio*,const ws::ProjectAction&);
void save(Studio*);
void finishMidi(Studio*);
void releaseKeyboard(Studio*,int,bool external=false);
void notice(Studio* s,const std::string& text){
 if(s->status)gtk_label_set_text(GTK_LABEL(s->status),text.c_str());
 if(s->toast)adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(s->toast),adw_toast_new(text.c_str()));
}
void change(Studio* s){s->dirty=true;refresh(s);}
void checkpoint(Studio* s){s->history.checkpoint(s->project);}
void finishMidi(Studio* s){
 if(!s->midi.recording())return;
 std::vector<ws::MidiNote> captured;
 try{captured=s->midi.finish(g_get_monotonic_time());}
 catch(const std::exception& e){
  g_warning("MIDI buffer could not finalize: %s",e.what());
  notice(s,tr("Not enough memory to finalize MIDI take",
              "Inte tillräckligt med minne för att avsluta MIDI-tagningen"));
  return;
 }
 if(s->midiButton)gtk_button_set_label(GTK_BUTTON(s->midiButton),tr("Record MIDI","Spela in MIDI"));
 if(captured.empty()){
  notice(s,tr("MIDI recording stopped – no notes","MIDI-inspelning stoppad – inga toner"));
  return;
 }
 if(s->midiTrack>=s->project.tracks.size())return;
 try{
  ws::Project result=s->project;
  // The project is limited to 20,000 seconds; grow it only when a note
  // actually extends past its previous duration.
  for(const auto& note:captured)
   result.durationSeconds=std::min(20000.,std::max(result.durationSeconds,
                                                 note.atSeconds+note.durationSeconds+.01));
  std::size_t inserted=0;
  for(const auto& note:captured)
   if(ws::insertNote(result,s->midiTrack,note,{4,0}).changed)++inserted;
  if(inserted){
   checkpoint(s);s->project=std::move(result);
   s->selected=s->midiTrack;s->selectedNote.reset();
   change(s);
   notice(s,tr("MIDI take added – Undo is available","MIDI-tagning tillagd – Ångra är tillgängligt"));
  }else notice(s,tr("No new MIDI notes – existing music unchanged",
                    "Inga nya MIDI-toner – den befintliga musiken är oförändrad"));
 }catch(const std::exception& e){
  g_warning("MIDI take failed: %s",e.what());
  notice(s,tr("Could not add MIDI take","Kunde inte lägga till MIDI-tagningen"));
 }
}
void toggleMidi(Studio* s){
 if(s->midi.recording()){finishMidi(s);return;}
 if(s->playback.active()||s->recording.active()||!s->recordFile.empty()){
  notice(s,tr("Stop playback or microphone recording before recording MIDI",
              "Stoppa uppspelning eller mikrofoninspelning innan du spelar in MIDI"));
  return;
 }
 if(s->selected>=s->project.tracks.size())return;
 if(!s->midi.begin(s->playhead,g_get_monotonic_time())){
  notice(s,tr("MIDI recording could not start","Kunde inte starta MIDI-inspelning"));return;
 }
 s->midiTrack=s->selected;
 if(s->midiButton)gtk_button_set_label(GTK_BUTTON(s->midiButton),
                                       tr("Stop MIDI recording","Stoppa MIDI-inspelning"));
 notice(s,tr("MIDI recording – play the keyboard, then Stop MIDI",
             "MIDI-inspelning – spela på tangenterna och tryck sedan Stoppa MIDI"));
}
void pressKeyboard(Studio* s,int key,float velocity=.8f,bool external=false){
 if(key<0||key>127)return;
 auto& held=external?s->externalKeys:s->activeKeys;
 auto& other=external?s->activeKeys:s->externalKeys;
 const auto i=static_cast<std::size_t>(key);
 if(held[i])return;
 held[i]=true;
 if(other[i])return; // one note-on for a key held by both sources
 if(s->midi.recording())s->midi.noteOn(key,g_get_monotonic_time(),velocity);
 if(s->keyboard&&key>=ws::keyboardLowest&&key<=ws::keyboardHighest)
  gtk_widget_queue_draw(s->keyboard);
 if(s->keyboardStatus)
  gtk_label_set_text(GTK_LABEL(s->keyboardStatus),ws::pitchName(key).c_str());
 if(s->selected>=s->project.tracks.size()||s->recording.active()||
    s->playback.active()||s->previewUnavailable)return;
 try{
  // Monophonic preview. Recording still captures polyphonic MIDI notes.
  s->audition.stop();
  ws::Project preview;
  preview.sampleRate=s->project.sampleRate;
  preview.durationSeconds=.60;
  ws::Track track;track.instrument=s->project.tracks[s->selected].instrument;
  track.notes.push_back({key,.005,.52,velocity});
  preview.tracks.push_back(std::move(track));
  s->audition.start(preview,0);
 }catch(const std::exception& e){
  g_warning("Keyboard preview failed: %s",e.what());s->previewUnavailable=true;
  notice(s,tr("No audio output – MIDI recording still works",
              "Ingen ljudutgång – MIDI-inspelning fungerar ändå"));
 }
}
void releaseKeyboard(Studio* s,int key,bool external){
 if(key<0||key>127)return;
 auto& held=external?s->externalKeys:s->activeKeys;
 auto& other=external?s->activeKeys:s->externalKeys;
 const auto i=static_cast<std::size_t>(key);
 if(!held[i])return;
 held[i]=false;
 if(other[i])return; // the other input source is still holding this note
 if(s->midi.recording()){
  try{
   if(!s->midi.noteOff(key,g_get_monotonic_time())&&
      s->midi.recorded()>=ws::MidiTake::maxTakeNotes){
    finishMidi(s);
    notice(s,tr("MIDI note limit reached – take preserved",
                "Gränsen för MIDI-toner nådd – tagningen sparad i projektet"));
   }
  }catch(const std::exception& e){
   g_warning("MIDI note buffering failed: %s",e.what());
   notice(s,tr("MIDI recording memory limit reached",
               "Minnesgränsen för MIDI-inspelning har nåtts"));
  }
 }
 if(s->keyboard&&key>=ws::keyboardLowest&&key<=ws::keyboardHighest)
  gtk_widget_queue_draw(s->keyboard);
 s->audition.stop();
 if(!s->audition.error().empty()&&!s->previewUnavailable){
  s->previewUnavailable=true;
  notice(s,tr("No audio output – MIDI recording still works",
              "Ingen ljudutgång – MIDI-inspelning fungerar ändå"));
 }
}
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
 if(s->midi.recording()){notice(s,tr("Stop MIDI recording first","Stoppa MIDI-inspelningen först"));return;}
 try{if(!s->recordFile.empty())finishRecord(s);s->playback.start(s->project,std::min(s->playhead,std::max(0.,s->project.durationSeconds-.01)));
 startTick(s);notice(s,tr("Playing","Spelar"));}catch(const std::exception& e){notice(s,e.what());}
}
void stop(Studio* s){finishMidi(s);s->audition.stop();s->playback.stop();if(!s->recordFile.empty())finishRecord(s);notice(s,tr("Stopped","Stoppad"));}
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
void exportNoteMap(Studio* s,const std::string& output){
 if(s->file.empty()||s->dirty){
  notice(s,tr("Save the project first, then export the 3D note map.",
              "Spara projektet först och exportera sedan 3D-notkartan."));
  return;
 }
 if(fs::exists(fs::u8path(output))){
  notice(s,tr("Choose a new image filename; the existing file will not be overwritten.",
              "Välj ett nytt bildnamn. Den befintliga filen skrivs inte över."));
  return;
 }
 std::error_code ec;
 const auto exe=fs::read_symlink("/proc/self/exe",ec);
 const auto helper=exe.parent_path()/"wenuy-studio-gfx";
 if(ec||!fs::exists(helper)){
  notice(s,tr("WenuyGFX note-map tool is not installed.",
              "Verktyget för WenuyGFX-notkartan är inte installerat."));
  return;
 }
 const std::string executable=helper.u8string();
 std::vector<const char*> args{executable.c_str(),s->file.c_str(),output.c_str()};
 const char* perf=std::getenv("WENUYOS_PERF");
 if(perf&&std::strcmp(perf,"low")==0)args.push_back("--low");
 args.push_back(nullptr);
 GError* error=nullptr;
 GSubprocess* process=g_subprocess_newv(args.data(),G_SUBPROCESS_FLAGS_NONE,&error);
 if(!process){
  if(error){g_warning("WenuyGFX launch: %s",error->message);g_error_free(error);}
  notice(s,tr("Could not start the 3D note-map tool.",
              "Kunde inte starta verktyget för 3D-notkartan."));
  return;
 }
 auto* request=new MapRequest(s->toast);
 g_subprocess_wait_async(process,nullptr,+[](GObject* process,GAsyncResult* result,gpointer data){
  std::unique_ptr<MapRequest> request(static_cast<MapRequest*>(data));
  GError* error=nullptr;
  const gboolean ok=g_subprocess_wait_check_finish(G_SUBPROCESS(process),result,&error);
  if(error){g_warning("WenuyGFX note-map export: %s",error->message);g_error_free(error);}
  GObject* obj=static_cast<GObject*>(g_weak_ref_get(&request->overlay));
  if(obj){
   adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(obj),adw_toast_new(
    ok?tr("3D note map exported","3D-notkartan exporterad"):
       tr("Could not export the 3D note map","Kunde inte exportera 3D-notkartan")));
   g_object_unref(obj);
  }
 },request);
 g_object_unref(process);
 notice(s,tr("Rendering 3D note map…","Ritar 3D-notkarta…"));
}
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
   case Pick::ExportGfx:
    exportNoteMap(s,path);break;
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
 const bool save=pick==Pick::Save||pick==Pick::ExportWav||pick==Pick::ExportMidi||
                 pick==Pick::ExportGfx||pick==Pick::Record;
 const char* title=pick==Pick::Open?tr("Open project","Öppna projekt"):
 pick==Pick::Save?tr("Save project","Spara projekt"):
 pick==Pick::ImportWav?tr("Import audio","Importera ljud"):
 pick==Pick::ExportWav?tr("Export WAV","Exportera WAV"):
 pick==Pick::ImportMidi?tr("Import MIDI","Importera MIDI"):
 pick==Pick::ExportMidi?tr("Export MIDI","Exportera MIDI"):
 pick==Pick::ExportGfx?tr("Export 3D note map","Exportera 3D-notkarta"):tr("Record WAV","Spela in WAV");
 GtkFileChooserNative* dialog=gtk_file_chooser_native_new(title,GTK_WINDOW(s->window),
 save?GTK_FILE_CHOOSER_ACTION_SAVE:GTK_FILE_CHOOSER_ACTION_OPEN,
 save?tr("Save","Spara"):tr("Open","Öppna"),tr("Cancel","Avbryt"));
 if(save){gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
 pick==Pick::Save?"song.wstudio":pick==Pick::ExportMidi?"song.mid":
 pick==Pick::ExportGfx?"notes-3d.ppm":pick==Pick::Record?"recording.wav":"mix.wav");}
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
void save(Studio* s){
 if(s->midi.recording()){
  notice(s,tr("Stop MIDI recording before saving the project",
              "Stoppa MIDI-inspelningen innan du sparar projektet"));
  return;
 }
 if(s->file.empty())choose(s,Pick::Save);else fileAction(s,Pick::Save,s->file);
}

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
 const bool blocked=s->recording.active()||!s->recordFile.empty()||s->chooserOpen||s->midi.recording();
 const auto result=s->guard.request(action,s->dirty,blocked);
 if(result==ws::GuardDecision::Busy){
  notice(s,blocked&&(s->recording.active()||!s->recordFile.empty()||s->midi.recording())?
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
void record(Studio* s){
 if(s->midi.recording()){
  notice(s,tr("Stop MIDI recording before microphone recording",
              "Stoppa MIDI-inspelningen innan du spelar in mikrofonljud"));
  return;
 }
 if(s->recording.active())finishRecord(s);else choose(s,Pick::Record);
}
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
void drawKeyboard(GtkDrawingArea* area,cairo_t* cr,int width,int height,gpointer user){
 auto* s=static_cast<Studio*>(user);
 GdkRGBA foreground{};
 gtk_style_context_get_color(gtk_widget_get_style_context(GTK_WIDGET(area)),&foreground);
 auto color=[&](double alpha){
  cairo_set_source_rgba(cr,foreground.red,foreground.green,foreground.blue,alpha);
 };
 const double white=double(width)/ws::keyboardWhiteCount;
 // Use the current GTK foreground color with differing alpha; no hardcoded
 // theme RGB values, custom skin, image atlas or graphics dependency.
 for(int i=0;i<ws::keyboardWhiteCount;++i){
  const int key=ws::keyboardWhiteKey(i);
  const double x=i*white;
  color((s->activeKeys[static_cast<std::size_t>(key)]||
         s->externalKeys[static_cast<std::size_t>(key)])?.27:.055);
  cairo_rectangle(cr,x+.7,.7,white-1.4,height-1.4);cairo_fill(cr);
  color(.55);cairo_set_line_width(cr,1);
  cairo_rectangle(cr,x+.7,.7,white-1.4,height-1.4);cairo_stroke(cr);
  color(.95);cairo_set_font_size(cr,11);
  cairo_move_to(cr,x+4,height-8);
  cairo_show_text(cr,ws::pitchName(key).c_str());
 }
 for(int i=0;i<ws::keyboardWhiteCount;++i){
  const int key=ws::keyboardBlackAfter(i);
  if(key<0)continue;
  const double x=(i+1-ws::keyboardBlackWidth*.5)*white;
  color((s->activeKeys[static_cast<std::size_t>(key)]||
         s->externalKeys[static_cast<std::size_t>(key)])?.98:.75);
  cairo_rectangle(cr,x,0,white*ws::keyboardBlackWidth,
                  height*ws::keyboardBlackHeight);cairo_fill(cr);
 }
}
void releaseExternalKeys(Studio* s){
 for(int key=0;key<128;++key)
  if(s->externalKeys[static_cast<std::size_t>(key)])
   releaseKeyboard(s,key,true);
}
void refreshExternalMidi(Studio* s){
 if(s->midiInput.connected()){
  notice(s,tr("Disconnect MIDI before refreshing the device list",
              "Koppla från MIDI innan du söker efter enheter igen"));
  return;
 }
 if(!s->midiDeviceNames)return;
 std::string error;
 const auto sources=s->midiInput.scan(error);
 if(!error.empty()){
  notice(s,tr("Could not access ALSA MIDI devices. Check device permissions.",
              "Kunde inte läsa ALSA MIDI-enheter. Kontrollera enhetens behörigheter."));
  return;
 }
 s->midiSources=sources;
 gtk_string_list_splice(s->midiDeviceNames,0,
                        g_list_model_get_n_items(G_LIST_MODEL(s->midiDeviceNames)),
                        nullptr);
 for(const auto& source:sources){
  char* safe=g_utf8_make_valid(source.label.c_str(),-1);
  gtk_string_list_append(s->midiDeviceNames,safe);
  g_free(safe);
 }
 if(!sources.empty())gtk_drop_down_set_selected(GTK_DROP_DOWN(s->midiDevicePicker),0);
 if(s->midiDeviceStatus){
  const std::string label=std::string(tr("MIDI devices found: ","MIDI-enheter hittade: "))+std::to_string(sources.size());
  gtk_label_set_text(GTK_LABEL(s->midiDeviceStatus),label.c_str());
 }
 gtk_widget_set_sensitive(s->midiDevicePicker,!sources.empty());
 gtk_widget_set_sensitive(s->midiDeviceButton,!sources.empty());
}
void toggleExternalMidi(Studio* s){
 if(s->midiInput.connected()){
  s->midiInput.disconnect();releaseExternalKeys(s);
  if(s->midiDeviceButton)
   gtk_button_set_label(GTK_BUTTON(s->midiDeviceButton),tr("Connect MIDI","Anslut MIDI"));
  if(s->midiDeviceStatus)
   gtk_label_set_text(GTK_LABEL(s->midiDeviceStatus),tr("MIDI disconnected","MIDI frånkopplat"));
  gtk_widget_set_sensitive(s->midiDevicePicker,!s->midiSources.empty());
  return;
 }
 if(!s->midiDevicePicker)return;
 const auto selected=gtk_drop_down_get_selected(GTK_DROP_DOWN(s->midiDevicePicker));
 if(selected>=s->midiSources.size()){
  notice(s,tr("Select a MIDI device first","Välj först en MIDI-enhet"));return;
 }
 std::string error;
 const auto source=s->midiSources[selected];
 const bool ok=s->midiInput.connect(source,[s](const ws::MidiInputMessage& event){
  switch(event.kind){
   case ws::MidiInputKind::NoteOn:
    pressKeyboard(s,event.key,event.velocity,true);break;
   case ws::MidiInputKind::NoteOff:
    releaseKeyboard(s,event.key,true);break;
   case ws::MidiInputKind::AllNotesOff:
    releaseExternalKeys(s);break;
   case ws::MidiInputKind::Disconnected:
    releaseExternalKeys(s);
    if(s->midiDeviceButton)
     gtk_button_set_label(GTK_BUTTON(s->midiDeviceButton),tr("Connect MIDI","Anslut MIDI"));
    if(s->midiDeviceStatus)
     gtk_label_set_text(GTK_LABEL(s->midiDeviceStatus),
                       tr("MIDI source disconnected","MIDI-källan kopplades från"));
    if(s->midiDevicePicker)gtk_widget_set_sensitive(s->midiDevicePicker,TRUE);
    notice(s,tr("MIDI device was disconnected","MIDI-enheten kopplades från"));
    break;
  }
 },error);
 if(!ok){
  g_warning("MIDI connect failed: %s",error.c_str());
  notice(s,tr("Could not connect to the selected MIDI device",
              "Kunde inte ansluta till den valda MIDI-enheten"));
  return;
 }
 gtk_button_set_label(GTK_BUTTON(s->midiDeviceButton),
                      tr("Disconnect MIDI","Koppla från MIDI"));
 gtk_widget_set_sensitive(s->midiDevicePicker,FALSE);
 if(s->midiDeviceStatus){
  const std::string label=std::string(tr("Connected to MIDI: ","Ansluten till MIDI: "))+source.label;
  char* safe=g_utf8_make_valid(label.c_str(),-1);
  gtk_label_set_text(GTK_LABEL(s->midiDeviceStatus),safe);g_free(safe);
 }
}
GtkWidget* createExternalMidiUi(Studio* s){
 auto* container=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
 gtk_widget_set_margin_start(container,10);
 gtk_widget_set_margin_end(container,10);
 auto* label=gtk_label_new(tr(
  "External MIDI: Scan, choose a local source, then Connect. No automatic connection.",
  "Extern MIDI: Sök, välj en lokal källa och tryck Anslut. Ingen automatisk anslutning."));
 gtk_label_set_wrap(GTK_LABEL(label),TRUE);
 gtk_label_set_xalign(GTK_LABEL(label),0);
 gtk_widget_add_css_class(label,"dim-label");
 gtk_box_append(GTK_BOX(container),label);
 auto* controls=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6);
 auto* scan=gtk_button_new_with_label(tr("Scan MIDI devices","Sök MIDI-enheter"));
 gtk_widget_set_tooltip_text(scan,tr("List local ALSA MIDI sources",
                                      "Lista lokala ALSA MIDI-källor"));
 g_signal_connect_swapped(scan,"clicked",G_CALLBACK(refreshExternalMidi),s);
 gtk_box_append(GTK_BOX(controls),scan);
 s->midiDeviceNames=gtk_string_list_new(nullptr);
 s->midiDevicePicker=gtk_drop_down_new(G_LIST_MODEL(s->midiDeviceNames),nullptr);
 g_object_unref(s->midiDeviceNames); // dropdown owns this list model
 gtk_widget_set_hexpand(s->midiDevicePicker,TRUE);
 gtk_widget_set_sensitive(s->midiDevicePicker,FALSE);
 gtk_accessible_update_property(GTK_ACCESSIBLE(s->midiDevicePicker),
   GTK_ACCESSIBLE_PROPERTY_LABEL,tr("External MIDI source","Extern MIDI-källa"),-1);
 gtk_box_append(GTK_BOX(controls),s->midiDevicePicker);
 s->midiDeviceButton=gtk_button_new_with_label(tr("Connect MIDI","Anslut MIDI"));
 g_signal_connect_swapped(s->midiDeviceButton,"clicked",G_CALLBACK(toggleExternalMidi),s);
 gtk_widget_set_sensitive(s->midiDeviceButton,FALSE);
 gtk_box_append(GTK_BOX(controls),s->midiDeviceButton);
 gtk_box_append(GTK_BOX(container),controls);
 s->midiDeviceStatus=gtk_label_new(s->midiInput.supported()?
   tr("Click Scan MIDI devices to search","Tryck Sök MIDI-enheter för att söka"):
   tr("ALSA MIDI input unavailable; on-screen keyboard still works",
      "ALSA MIDI-indata saknas; det virtuella keyboardet fungerar ändå"));
 gtk_label_set_wrap(GTK_LABEL(s->midiDeviceStatus),TRUE);
 gtk_label_set_xalign(GTK_LABEL(s->midiDeviceStatus),0);
 gtk_widget_add_css_class(s->midiDeviceStatus,"dim-label");
 gtk_box_append(GTK_BOX(container),s->midiDeviceStatus);
 gtk_widget_set_sensitive(scan,s->midiInput.supported());
 return container;
}
GtkWidget* createKeyboard(Studio* s){
 auto* box=gtk_box_new(GTK_ORIENTATION_VERTICAL,3);
 auto* instruction=gtk_label_new(tr(
  "Play C4–B5 with mouse or Z/X/C… and Q/W/E… keys. Record MIDI to save played notes.",
  "Spela C4–B5 med mus eller Z/X/C… och Q/W/E… Tryck Spela in MIDI för att spara toner."));
 gtk_label_set_wrap(GTK_LABEL(instruction),TRUE);
 gtk_label_set_xalign(GTK_LABEL(instruction),0);
 gtk_widget_set_margin_start(instruction,10);
 gtk_widget_set_margin_end(instruction,10);
 gtk_widget_add_css_class(instruction,"dim-label");
 gtk_box_append(GTK_BOX(box),instruction);
 auto* scroll=gtk_scrolled_window_new();
 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_NEVER);
 gtk_widget_set_size_request(scroll,-1,112);
 auto* draw=gtk_drawing_area_new();
 s->keyboard=draw;
 gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(draw),560);
 gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(draw),106);
 gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(draw),drawKeyboard,s,nullptr);
 gtk_widget_set_focusable(draw,TRUE);
 gtk_accessible_update_property(GTK_ACCESSIBLE(draw),GTK_ACCESSIBLE_PROPERTY_LABEL,
  tr("Virtual keyboard C4 to B5 – press a key to hear it",
     "Virtuellt keyboard C4 till B5 – tryck en tangent för att höra den"),-1);
 auto* pointer=gtk_gesture_click_new();
 gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(pointer),GDK_BUTTON_PRIMARY);
 g_signal_connect(pointer,"pressed",G_CALLBACK(+[](GtkGestureClick*,gint,double x,double y,gpointer data){
  auto* st=static_cast<Studio*>(data);
  gtk_widget_grab_focus(st->keyboard);
  if(auto key=ws::keyboardKeyAt(x,y,gtk_widget_get_width(st->keyboard),
                                     gtk_widget_get_height(st->keyboard))){
   st->pointerKey=*key;pressKeyboard(st,*key);
  }
 }),s);
 g_signal_connect(pointer,"released",G_CALLBACK(+[](GtkGestureClick*,gint,double,double,gpointer data){
  auto* st=static_cast<Studio*>(data);
  if(st->pointerKey>=0)releaseKeyboard(st,st->pointerKey);
  st->pointerKey=-1;
 }),s);
 gtk_widget_add_controller(draw,GTK_EVENT_CONTROLLER(pointer));
 auto* keys=gtk_event_controller_key_new();
 g_signal_connect(keys,"key-pressed",G_CALLBACK(+[](GtkEventControllerKey*,guint keyval,
     guint,GdkModifierType mod,gpointer data)->gboolean{
  if(mod&(GDK_CONTROL_MASK|GDK_ALT_MASK|GDK_SUPER_MASK))return FALSE;
  auto* st=static_cast<Studio*>(data);
  const int key=ws::keyboardComputerKey(gdk_keyval_to_lower(keyval));
  if(key<0)return FALSE;
  pressKeyboard(st,key);return TRUE;
 }),s);
 g_signal_connect(keys,"key-released",G_CALLBACK(+[](GtkEventControllerKey*,guint keyval,
     guint,GdkModifierType,gpointer data){
  auto* st=static_cast<Studio*>(data);
  releaseKeyboard(st,ws::keyboardComputerKey(gdk_keyval_to_lower(keyval)));
 }),s);
 gtk_widget_add_controller(draw,keys);
 auto* focus=gtk_event_controller_focus_new();
 g_signal_connect(focus,"leave",G_CALLBACK(+[](GtkEventControllerFocus*,gpointer data){
  auto* st=static_cast<Studio*>(data);
  for(int key=ws::keyboardLowest;key<=ws::keyboardHighest;++key)releaseKeyboard(st,key);
  st->pointerKey=-1;
 }),s);
 gtk_widget_add_controller(draw,focus);
 gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),draw);
 gtk_box_append(GTK_BOX(box),scroll);
 s->keyboardStatus=gtk_label_new(tr("Keyboard ready","Keyboard redo"));
 gtk_label_set_xalign(GTK_LABEL(s->keyboardStatus),0);
 gtk_widget_set_margin_start(s->keyboardStatus,10);
 gtk_widget_add_css_class(s->keyboardStatus,"dim-label");
 gtk_box_append(GTK_BOX(box),s->keyboardStatus);
 return box;
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
  if(!st->dirty&&!st->recording.active()&&!st->midi.recording()&&st->recordFile.empty()&&!st->chooserOpen&&!st->guard.hasPending())return FALSE;
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
   else if(g_strcmp0(action,"export-gfx")==0)choose(st,Pick::ExportGfx);
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
 // Only advertise the optional helper when it is installed alongside the app.
 std::error_code gfxError;
 const auto runningExe=fs::read_symlink("/proc/self/exe",gfxError);
 if(!gfxError&&fs::exists(runningExe.parent_path()/"wenuy-studio-gfx"))
  addAction(files,"export-gfx",tr("Export 3D note map…","Exportera 3D-notkarta…"));
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
 s->midiButton=button(pianoBar,s,tr("Record MIDI","Spela in MIDI"),toggleMidi);
 gtk_box_append(GTK_BOX(pianoPage),pianoBar);
 auto* hint=gtk_label_new(tr("Click: add/select note · Right-click/Delete: remove · Arrows: move",
                          "Klick: lägg till/välj ton · Högerklick/Delete: ta bort · Pilar: flytta"));
 gtk_label_set_wrap(GTK_LABEL(hint),TRUE);
 gtk_label_set_xalign(GTK_LABEL(hint),0);
 gtk_widget_set_margin_start(hint,10);gtk_widget_set_margin_end(hint,10);
 gtk_widget_add_css_class(hint,"dim-label");
 gtk_box_append(GTK_BOX(pianoPage),hint);
 gtk_box_append(GTK_BOX(pianoPage),createKeyboard(s));
 gtk_box_append(GTK_BOX(pianoPage),createExternalMidiUi(s));
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
