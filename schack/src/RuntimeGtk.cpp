// Wenuy Schack 0.3.0 – Native GTK4 runtime front end, designed for a Debian 13
// build host without GTK development packages. GTK4 is a runtime dependency
// of WenuyOS; only stable GTK4/GIO entry points are used.
#include "Chess.h"
#include "LanChess.h"
#include "SystemIntegration.h"
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/random.h>
#include <iomanip>
#include "Lessons.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
// Keep a direct GTK4 dependency so WenuyOS verify can recognize this as a GUI application.
unsigned int gtk_get_major_version(void);
// GLib and GIO ABI: these signatures are stable on Debian 13 / WenuyOS.
unsigned long g_signal_connect_data(void*, const char*, void (*)(), void*, void (*)(void*,void*), unsigned int);
int g_application_run(void*, int, char**);
void g_object_unref(void*);
unsigned int g_idle_add(int (*)(void*),void*);
unsigned int g_timeout_add(unsigned int,int (*)(void*),void*);
// Cairo 1.x public ABI; the runtime uses GTK4's Cairo drawing surface.
struct _cairo; typedef _cairo cairo_t;
void cairo_save(cairo_t*); void cairo_restore(cairo_t*);
void cairo_translate(cairo_t*, double, double); void cairo_scale(cairo_t*, double, double);
void cairo_set_source_rgba(cairo_t*, double,double,double,double);
void cairo_rectangle(cairo_t*,double,double,double,double); void cairo_fill(cairo_t*);
void cairo_fill_preserve(cairo_t*); void cairo_stroke(cairo_t*); void cairo_new_path(cairo_t*);
void cairo_move_to(cairo_t*,double,double); void cairo_line_to(cairo_t*,double,double);
void cairo_curve_to(cairo_t*,double,double,double,double,double,double);
void cairo_close_path(cairo_t*); void cairo_arc(cairo_t*,double,double,double,double,double);
void cairo_set_line_width(cairo_t*,double); void cairo_set_line_join(cairo_t*,int);
void cairo_set_line_cap(cairo_t*,int); void cairo_select_font_face(cairo_t*,const char*,int,int);
void cairo_set_font_size(cairo_t*,double); void cairo_show_text(cairo_t*,const char*);
}

namespace {
using Ptr=void*;
// Use WenuyOS's shared gettext domain (and Swedish/English fallbacks).
// Other locales read se.wenuyos.app.schack.mo when installed by the system.
const char* tr(const char* en,const char* sv){
 return schack_system::translate(en,sv);
}

#define GTK_SYMBOLS(M) \
 M(Ptr, gtk_application_new, (const char*,int)) \
 M(Ptr, gtk_application_window_new, (Ptr)) \
 M(void,gtk_window_set_title,(Ptr,const char*)) \
 M(void,gtk_window_set_default_size,(Ptr,int,int)) \
 M(void,gtk_window_set_child,(Ptr,Ptr)) \
 M(void,gtk_window_present,(Ptr)) \
 M(Ptr,gtk_box_new,(int,int)) \
 M(void,gtk_orientable_set_orientation,(Ptr,int)) \
 M(int,gtk_widget_get_width,(Ptr)) \
 M(void,gtk_box_append,(Ptr,Ptr)) \
 M(Ptr,gtk_label_new,(const char*)) \
 M(Ptr,gtk_entry_new,()) \
 M(void,gtk_entry_set_placeholder_text,(Ptr,const char*)) \
 M(const char*,gtk_editable_get_text,(Ptr)) \
 M(void,gtk_editable_set_text,(Ptr,const char*)) \
 M(void,gtk_label_set_label,(Ptr,const char*)) \
 M(void,gtk_label_set_xalign,(Ptr,float)) \
 M(void,gtk_label_set_yalign,(Ptr,float)) \
 M(void,gtk_label_set_wrap,(Ptr,int)) \
 M(Ptr,gtk_button_new_with_label,(const char*)) \
 M(void,gtk_button_set_label,(Ptr,const char*)) \
 M(Ptr,gtk_drawing_area_new,()) \
 M(void,gtk_drawing_area_set_draw_func,(Ptr,void(*)(Ptr,cairo_t*,int,int,void*),void*,void(*)(void*))) \
 M(Ptr,gtk_gesture_click_new,()) \
 M(Ptr,gtk_gesture_drag_new,()) \
 M(void,gtk_gesture_single_set_button,(Ptr,unsigned int)) \
 M(void,gtk_widget_add_controller,(Ptr,Ptr)) \
 M(Ptr,gtk_event_controller_key_new,()) \
 M(void,gtk_widget_set_vexpand,(Ptr,int)) \
 M(void,gtk_widget_set_hexpand,(Ptr,int)) \
 M(void,gtk_widget_set_size_request,(Ptr,int,int)) \
 M(void,gtk_widget_set_margin_start,(Ptr,int)) \
 M(void,gtk_widget_set_margin_end,(Ptr,int)) \
 M(void,gtk_widget_set_margin_top,(Ptr,int)) \
 M(void,gtk_widget_set_margin_bottom,(Ptr,int)) \
 M(void,gtk_widget_set_visible,(Ptr,int)) \
 M(void,gtk_widget_set_sensitive,(Ptr,int)) \
 M(void,gtk_widget_queue_draw,(Ptr)) \
 M(void,gtk_widget_set_focusable,(Ptr,int)) \
 M(int,gtk_widget_grab_focus,(Ptr)) \
 M(int,gtk_widget_has_focus,(Ptr)) \
 M(void,gtk_widget_add_css_class,(Ptr,const char*)) \
 M(Ptr,gtk_scrolled_window_new,()) \
 M(void,gtk_scrolled_window_set_child,(Ptr,Ptr)) \
 M(void,gtk_widget_set_tooltip_text,(Ptr,const char*)) \
 M(void,gtk_accessible_update_property,(Ptr,int,...))
struct GTK {
 void* lib=nullptr;
 #define DECL(ret,name,args) ret (*name) args = nullptr;
 GTK_SYMBOLS(DECL)
 #undef DECL
 Ptr (*gtk_css_provider_new)() = nullptr;
 void (*gtk_css_provider_load_from_data)(Ptr,const char*,long) = nullptr;
 Ptr (*gtk_widget_get_display)(Ptr) = nullptr;
 void (*gtk_style_context_add_provider_for_display)(Ptr,Ptr,unsigned) = nullptr;
 bool load() {
   lib=dlopen("libgtk-4.so.1",RTLD_NOW|RTLD_LOCAL);
   if(!lib){std::fprintf(stderr,"Wenuy Schack: GTK4-biblioteket saknas: %s\n",dlerror());return false;}
   #define LOAD(ret,name,args) name=reinterpret_cast<decltype(name)>(dlsym(lib,#name)); if(!name){std::fprintf(stderr,"Wenuy Schack: GTK4 saknar symbol %s\n",#name);return false;}
   GTK_SYMBOLS(LOAD)
   #undef LOAD
   // Appearance enhancements are optional; older GTK4 runtimes still work.
   gtk_css_provider_new=reinterpret_cast<decltype(gtk_css_provider_new)>(dlsym(lib,"gtk_css_provider_new"));
   gtk_css_provider_load_from_data=reinterpret_cast<decltype(gtk_css_provider_load_from_data)>(dlsym(lib,"gtk_css_provider_load_from_data"));
   gtk_widget_get_display=reinterpret_cast<decltype(gtk_widget_get_display)>(dlsym(lib,"gtk_widget_get_display"));
   gtk_style_context_add_provider_for_display=reinterpret_cast<decltype(gtk_style_context_add_provider_for_display)>(dlsym(lib,"gtk_style_context_add_provider_for_display"));
   return true;
 }
} g;
#undef GTK_SYMBOLS

template <typename Fn> void connect(Ptr obj,const char* signal,Fn callback,void* data){
 g_signal_connect_data(obj,signal,reinterpret_cast<void(*)()>(callback),data,nullptr,0);
}
void rgba(cairo_t* c,double r,double gg,double b,double a=1){cairo_set_source_rgba(c,r,gg,b,a);}
void outline(cairo_t* c,bool white){
 rgba(c,white?.975:.135,white?.98:.195,white?.99:.285);cairo_fill_preserve(c);
 rgba(c,white?.14:.82,white?.21:.88,white?.3:.93);cairo_set_line_width(c,2.9);cairo_stroke(c);
}
void base(cairo_t* c,bool white){
 // A two-level, scale-independent plinth adds depth without textures or blur.
 cairo_new_path(c);cairo_move_to(c,30,75);cairo_line_to(c,70,75);
 cairo_line_to(c,74,81);cairo_line_to(c,26,81);cairo_close_path(c);outline(c,white);
 cairo_new_path(c);cairo_move_to(c,24,81);cairo_line_to(c,76,81);
 cairo_curve_to(c,80,81,81,86,79,89);cairo_line_to(c,21,89);
 cairo_curve_to(c,19,86,20,81,24,81);cairo_close_path(c);outline(c,white);
 rgba(c,white?.50:.71,white?.59:.77,white?.67:.84,.78);
 cairo_set_line_width(c,1.9);cairo_move_to(c,27,85);cairo_line_to(c,73,85);cairo_stroke(c);
}
void piece(cairo_t* c,char p,double x,double y,double s){
 if(!p)return;
 const bool w=std::isupper(static_cast<unsigned char>(p));
 const char k=static_cast<char>(std::tolower(static_cast<unsigned char>(p)));
 cairo_save(c);cairo_translate(c,x+s*.065,y+s*.065);cairo_scale(c,s*.87/100,s*.87/100);
 cairo_set_line_join(c,1);cairo_set_line_cap(c,1);
 switch(k){
 case 'p':
  cairo_new_path(c);cairo_arc(c,50,33,15,0,6.28318530718);outline(c,w);
  cairo_new_path(c);cairo_move_to(c,41,48);cairo_curve_to(c,41,59,34,70,30,79);cairo_line_to(c,70,79);
  cairo_curve_to(c,66,70,59,59,59,48);cairo_close_path(c);outline(c,w);
  rgba(c,w?.61:.72,w?.67:.78,w?.74:.86,.8);
  cairo_set_line_width(c,2.4);
  cairo_move_to(c,40,64);cairo_curve_to(c,44,59,45,55,45,49);cairo_stroke(c);break;
 case 'r':
  cairo_new_path(c);cairo_move_to(c,27,27);cairo_line_to(c,27,17);cairo_line_to(c,37,17);
  cairo_line_to(c,37,25);cairo_line_to(c,45,25);cairo_line_to(c,45,17);
  cairo_line_to(c,55,17);cairo_line_to(c,55,25);cairo_line_to(c,63,25);
  cairo_line_to(c,63,17);cairo_line_to(c,73,17);cairo_line_to(c,73,27);
  cairo_line_to(c,69,34);cairo_line_to(c,65,75);cairo_line_to(c,35,75);
  cairo_line_to(c,31,34);cairo_close_path(c);outline(c,w);
  rgba(c,w?.58:.70,w?.64:.78,w?.72:.86,.84);
  cairo_set_line_width(c,2.5);
  cairo_move_to(c,36,37);cairo_line_to(c,64,37);cairo_stroke(c);
  cairo_move_to(c,39,68);cairo_line_to(c,61,68);cairo_stroke(c);
  // Two masonry seams sharpen the rook silhouette at small sizes.
  cairo_set_line_width(c,1.8);cairo_move_to(c,46,38);cairo_line_to(c,46,54);
  cairo_move_to(c,56,38);cairo_line_to(c,56,54);cairo_stroke(c);break;
 case 'n':
  cairo_new_path(c);cairo_move_to(c,29,78);cairo_curve_to(c,37,64,33,58,31,52);
  cairo_line_to(c,19,56);cairo_curve_to(c,13,55,13,47,17,43);
  cairo_line_to(c,30,29);cairo_line_to(c,34,15);cairo_line_to(c,44,22);
  cairo_curve_to(c,56,20,68,28,72,42);cairo_curve_to(c,77,53,72,65,67,78);
  cairo_close_path(c);outline(c,w);
  cairo_new_path(c);cairo_arc(c,35,38,3,0,6.28318530718);
  rgba(c,w?.14:.94,w?.23:.96,w?.34:.98);cairo_fill(c);
  // The mane and muzzle remain distinguishable at small square sizes.
  rgba(c,w?.56:.76,w?.64:.81,w?.72:.87,.85);cairo_set_line_width(c,2.8);
  cairo_move_to(c,50,29);cairo_curve_to(c,62,34,67,44,67,53);cairo_stroke(c);
  cairo_move_to(c,24,49);cairo_line_to(c,31,49);cairo_stroke(c);
  // Ear and bridle are simple Cairo strokes, not filtered assets.
  cairo_move_to(c,35,22);cairo_line_to(c,40,30);
  cairo_move_to(c,29,52);cairo_curve_to(c,37,56,42,57,47,58);
  cairo_stroke(c);break;
 case 'b':
  cairo_new_path(c);cairo_move_to(c,50,15);cairo_curve_to(c,67,31,69,43,61,54);
  cairo_curve_to(c,75,63,69,70,67,76);cairo_line_to(c,33,76);
  cairo_curve_to(c,31,70,25,63,39,54);cairo_curve_to(c,31,43,33,31,50,15);
  cairo_close_path(c);outline(c,w);
  cairo_new_path(c);cairo_move_to(c,58,27);cairo_line_to(c,44,54);
  rgba(c,w?.23:.8,w?.29:.86,w?.39:.92);cairo_set_line_width(c,4);cairo_stroke(c);
  cairo_set_line_width(c,2.0);cairo_move_to(c,37,65);cairo_line_to(c,63,65);
  cairo_stroke(c);break;
 case 'q':
  cairo_new_path(c);cairo_move_to(c,20,27);cairo_line_to(c,34,43);cairo_line_to(c,39,20);
  cairo_line_to(c,50,42);cairo_line_to(c,61,20);cairo_line_to(c,66,43);
  cairo_line_to(c,80,27);cairo_line_to(c,68,73);cairo_line_to(c,32,73);
  cairo_close_path(c);outline(c,w);
  for(int xx:{20,39,61,80}){cairo_new_path(c);cairo_arc(c,xx,(xx==20||xx==80)?24:18,4.2,0,6.28318530718);outline(c,w);}
  rgba(c,w?.58:.73,w?.65:.79,w?.72:.87,.9);cairo_set_line_width(c,2.6);
  cairo_move_to(c,36,62);cairo_line_to(c,64,62);cairo_stroke(c);
  cairo_set_line_width(c,1.8);
  cairo_move_to(c,39,56);cairo_line_to(c,61,56);cairo_stroke(c);break;
 case 'k':
  cairo_new_path(c);cairo_move_to(c,47,13);cairo_line_to(c,53,13);cairo_line_to(c,53,22);
  cairo_line_to(c,62,22);cairo_line_to(c,62,28);cairo_line_to(c,53,28);
  cairo_line_to(c,53,38);cairo_line_to(c,47,38);cairo_line_to(c,47,28);
  cairo_line_to(c,38,28);cairo_line_to(c,38,22);cairo_line_to(c,47,22);
  cairo_close_path(c);outline(c,w);
  cairo_new_path(c);cairo_move_to(c,45,36);cairo_curve_to(c,28,40,29,57,37,65);
  cairo_line_to(c,32,76);cairo_line_to(c,68,76);cairo_line_to(c,63,65);
  cairo_curve_to(c,71,57,72,40,55,36);cairo_close_path(c);outline(c,w);
  rgba(c,w?.58:.73,w?.65:.79,w?.72:.87,.9);cairo_set_line_width(c,2.5);
  cairo_move_to(c,39,63);cairo_line_to(c,61,63);cairo_stroke(c);
  cairo_set_line_width(c,1.8);cairo_move_to(c,44,51);
  cairo_curve_to(c,48,47,52,47,56,51);cairo_stroke(c);break;
 }
 base(c,w);cairo_restore(c);
}

struct App {
 Ptr window=nullptr,board=nullptr,status=nullptr,turn=nullptr,history=nullptr,levelBtn=nullptr,modeBtn=nullptr,sideBtn=nullptr,undoBtn=nullptr;
 Ptr themeBtn=nullptr,hintBtn=nullptr,analyzeBtn=nullptr,material=nullptr,lessonProgress=nullptr,cssProvider=nullptr;
 Ptr sidePanel=nullptr,focusBtn=nullptr,mainBox=nullptr,toolBar=nullptr,brand=nullptr,boardSummary=nullptr;
 Ptr lanStatus=nullptr,lanEntry=nullptr,lanCodeEntry=nullptr,lanHostBtn=nullptr,lanJoinBtn=nullptr,lanStopBtn=nullptr;
 Ptr lessonBox=nullptr,lessonTitle=nullptr,lessonTask=nullptr,lessonFeedback=nullptr,lessonProfile=nullptr,promoteBtn=nullptr;
 chess::Game game, savedGame;
 int selected=-1,cursor=12,skill=2,mode=0,human=0,palette=0,hintFrom=-1,hintTo=-1;
 int dragOrigin=-1; double dragX=0,dragY=0; char promotion='q';
 bool flipped=false,training=false,lessonSolved=false,busy=false,closed=false,thinkingHint=false;
 bool focusBoard=false,keyboardCursor=false,compactLayout=false,narrowToolbar=false;
 bool lanConnecting=false,analysisBusy=false;
 // 160 ms linear piece travel; no extra surfaces, textures or worker thread.
 bool movingPiece=false;
 int motionFrom=-1,motionTo=-1;
 char motionGlyph=0;
 std::chrono::steady_clock::time_point motionStarted{};
 std::string lanMessage;
 std::shared_ptr<chess::lan::Peer> lanPeer;
 std::shared_ptr<std::atomic<bool>> lanCancel;
 std::uint64_t lessonMask=0; std::array<unsigned,64> lessonMistakes{},lessonSuccesses{}; std::string hintText;
 std::size_t lessonIndex=0,lessonPly=0,lessonVariation=0; unsigned epoch=0; std::string transient;
};
App app;

std::string userDir(){
 const char* x=std::getenv("XDG_DATA_HOME");if(x&&*x)return std::string(x)+"/wenuy-schack";
 const char* h=std::getenv("HOME");return std::string(h&&*h?h:"/tmp")+"/.local/share/wenuy-schack";
}
void saveProgress(){
 try {
  std::filesystem::create_directories(userDir());
  const std::string path=userDir()+"/progress.txt";
  const std::string tmp=path+".tmp";
  const chess::LessonProgress progress{app.lessonMask,app.lessonMistakes,app.lessonSuccesses};
  {
   std::ofstream out(tmp,std::ios::trunc);
   chess::writeLessonProgress(out,progress);
   if(!out)return;
  }
  std::filesystem::rename(tmp,path);
 }catch(const std::exception& e){
  std::fprintf(stderr,"Schack: kunde inte spara övningar: %s\n",e.what());
 }
}
void loadProgress(){
 std::ifstream input(userDir()+"/progress.txt");
 chess::LessonProgress progress;
 if(!chess::readLessonProgress(input,progress))return;
 app.lessonMask=progress.completed;
 app.lessonMistakes=progress.mistakes;
 app.lessonSuccesses=progress.successes;
}
void save(){
 if(app.training||app.mode==2)return;
 try {
  std::filesystem::create_directories(userDir());
  std::ofstream o(userDir()+"/session.txt.tmp",std::ios::trunc);
  if(!o)return;
  o<<app.game.positions.front().fen()<<'\n'<<app.mode<<' '<<app.skill<<' '<<app.human<<' '<<app.flipped<<' '<<app.palette<<'\n';
  for(const auto& m:app.game.moves)o<<chess::uci(m)<<'\n';
  o.close();if(o)std::filesystem::rename(userDir()+"/session.txt.tmp",userDir()+"/session.txt");
 }catch(const std::exception& e){std::fprintf(stderr,"Schack: spara misslyckades: %s\n",e.what());}
}
void load(){
 std::ifstream i(userDir()+"/session.txt");if(!i)return;
 std::string fen,params;std::getline(i,fen);std::getline(i,params);
 chess::Game temp;if(!temp.loadFen(fen))return;
 int mode=0,skill=2,human=0,flip=0;
 std::istringstream p(params);if(!(p>>mode>>skill>>human>>flip))return;
 std::string move;while(i>>move){if(!temp.playUci(move))return;}
 app.game=std::move(temp);app.mode=mode==1?1:0;
 app.skill=std::clamp(skill,1,5);app.human=human==1?1:0;app.flipped=flip!=0;
 int palette=0;if(p>>palette)app.palette=std::clamp(palette,0,3);
}
struct Geom{double x,y,s;};
Geom boardGeom(int w,int h){
 const double s=std::max(7.,std::min((w-34.)/8.,(h-34.)/8.));
 return {(w-s*8)/2.,(h-s*8)/2.,s};
}
struct Palette{
 double bg[3],edge[3],light[3],dark[3],accent[3];
 const char* name;
};
const Palette palettes[4]={
 {{.062,.113,.181},{.23,.38,.54},{.925,.948,.963},{.338,.507,.672},{.16,.64,.95},"Nordiskt blått"},
 {{.875,.915,.95},{.52,.65,.77},{.992,.985,.954},{.52,.65,.70},{.075,.36,.69},"Ljust läge"},
 {{.16,.134,.117},{.42,.32,.25},{.96,.86,.69},{.65,.43,.29},{.12,.6,.72},"Klassiskt trä"},
 {{.035,.055,.085},{.81,.86,.90},{1,1,1},{.07,.14,.24},{1,.81,.14},"Hög kontrast"}
};
const Palette& colors(){return palettes[app.palette];}
void theme(){
 if(!app.window || !g.gtk_css_provider_new || !g.gtk_widget_get_display ||
    !g.gtk_css_provider_load_from_data || !g.gtk_style_context_add_provider_for_display)return;
 // Let GTK/WenuyOS provide application foreground and background colours.
 // Only the chessboard's four deliberate palettes are rendered in Cairo.
 const std::string css=std::string(".wenuy-topbar {padding: 2px 3px 5px;}")
  +".wenuy-panel {border-radius: 16px; padding: 12px; }"
  +".wenuy-control {border-radius: 9px; padding: 5px 9px; }"
  +".wenuy-brand {font-weight: 800; letter-spacing: 1.1px;}"
  +".wenuy-dim {opacity: .70;}";
 if(!app.cssProvider){
  app.cssProvider=g.gtk_css_provider_new();
  g.gtk_style_context_add_provider_for_display(g.gtk_widget_get_display(app.window),app.cssProvider,600);
 }
 g.gtk_css_provider_load_from_data(app.cssProvider,css.c_str(),-1);
}
// Use the actual allocated window width; portrait layouts keep controls below the board.
void responsive(){
 if(!app.window||!app.mainBox||!app.sidePanel)return;
 const int width=g.gtk_widget_get_width(app.window);
 if(width<=0)return;
 const bool compact=width<780;
 if(compact!=app.compactLayout){
  app.compactLayout=compact;
  g.gtk_orientable_set_orientation(app.mainBox,compact?1:0);
  g.gtk_widget_set_size_request(app.sidePanel,compact?-1:252,compact?168:-1);
  g.gtk_widget_set_vexpand(app.sidePanel,compact?0:1);
 }
 const bool tiny=width<520;
 if(tiny!=app.narrowToolbar){
  app.narrowToolbar=tiny;
  if(app.brand)g.gtk_widget_set_visible(app.brand,tiny?0:1);
  if(app.toolBar)g.gtk_orientable_set_orientation(app.toolBar,tiny?1:0);
 }
}
void onWindowWidth(Ptr,Ptr,void*){responsive();}
const char* lanErrorText(const std::string& error);
void clearHint(){app.hintFrom=app.hintTo=-1;app.hintText.clear();app.thinkingHint=false;}
void repaint(){if(app.board&&!app.closed)g.gtk_widget_queue_draw(app.board);}
void stopPieceMotion(){app.movingPiece=false;}
int motionFrame(void*){
 if(app.closed||!app.movingPiece)return 0;
 constexpr auto duration=std::chrono::milliseconds(160);
 if(std::chrono::steady_clock::now()-app.motionStarted>=duration){
  stopPieceMotion();repaint();return 0;
 }
 repaint();return 1;
}
void startPieceMotion(const chess::Move& move,char moving){
 stopPieceMotion();
 if(!schack_system::allowBoardMotion() || !moving ||
    move.from<0||move.from>=64||move.to<0||move.to>=64||move.from==move.to)return;
 app.motionFrom=move.from;app.motionTo=move.to;
 // Use the *resulting* piece for promotion; the board already has legal state.
 app.motionGlyph=app.game.board().squares[move.to];
 app.motionStarted=std::chrono::steady_clock::now();
 app.movingPiece=true;
 g_timeout_add(16,motionFrame,nullptr);
}
const char* pieceName(char p) {
 switch(std::tolower(static_cast<unsigned char>(p))) {
 case 'p':return tr("pawn","bonde");
 case 'n':return tr("knight","springare");
 case 'b':return tr("bishop","löpare");
 case 'r':return tr("rook","torn");
 case 'q':return tr("queen","dam");
 case 'k':return tr("king","kung");
 default:return tr("empty square","tom ruta");
 }
}
// The Cairo board is one GTK widget; provide a changing accessible name
// describing the focused square and selected piece, not merely a static tooltip.
void updateBoardAccessibility(){
 if(!app.board)return;
 const auto& b=app.game.board();
 const int square=std::clamp(app.cursor,0,63);
 const char pc=b.squares[square];
 std::string label=std::string(tr("Chessboard. ", "Schackbräde. "))+
    chess::squareName(square)+": ";
 if(pc){
  label+=(std::isupper(static_cast<unsigned char>(pc))?tr("White ","Vit "):tr("Black ","Svart "));
  label+=pieceName(pc);
 }else label+=tr("empty square","tom ruta");
 if(app.selected>=0){
  label+=tr(". Selected: ",". Vald: ");
  label+=chess::squareName(app.selected);
 }
 if(b.inCheck(b.white))label+=tr(". Check."," . Schack.");
 g.gtk_accessible_update_property(app.board,4,label.c_str(),-1); // GtkAccessibleProperty LABEL
}
void refresh(){
 if(app.closed||!app.window)return;
 if(app.sidePanel)g.gtk_widget_set_visible(app.sidePanel,app.focusBoard?0:1);
 if(app.focusBtn)g.gtk_button_set_label(app.focusBtn,app.focusBoard?tr("Show side panel","Visa sidopanel"):tr("Focus on board","Fokus på brädet"));
 if(app.undoBtn)g.gtk_button_set_label(app.undoBtn,app.training?tr("Restart exercise","Starta om övningen"):tr("Undo","Ångra"));
 if(app.lanStatus)g.gtk_label_set_label(app.lanStatus,app.lanMessage.c_str());
 if(app.lanHostBtn)g.gtk_widget_set_sensitive(app.lanHostBtn,!app.lanConnecting&&!app.lanPeer);
 if(app.lanJoinBtn)g.gtk_widget_set_sensitive(app.lanJoinBtn,!app.lanConnecting&&!app.lanPeer);
 if(app.lanStopBtn)g.gtk_widget_set_sensitive(app.lanStopBtn,app.lanConnecting||!!app.lanPeer);
 const std::string result=app.game.result();
 std::string text;
 if(result=="checkmate-white")text=tr("Checkmate – White wins","Schack matt – vit vinner");
 else if(result=="checkmate-black")text=tr("Checkmate – Black wins","Schack matt – svart vinner");
 else if(result=="stalemate")text=tr("Draw – stalemate","Remi – patt");
 else if(result=="fifty")text=tr("Draw – 50-move rule","Remi – 50-dragsregeln");
 else if(result=="repetition")text=tr("Draw – threefold repetition","Remi – trefaldig upprepning");
 else if(result=="material")text=tr("Draw – insufficient material","Remi – otillräckligt material");
 else if(app.training&&app.lessonSolved)text=tr("Correct! Choose the next lesson.","Rätt drag! Välj nästa övning.");
 else if(app.busy)text=tr("Computer is thinking…","Datorn tänker…");
 else if(app.thinkingHint)text=tr("Analysing a move… you can keep playing.","Analyserar ett drag… du kan fortsätta spela.");
 else if(app.analysisBusy)text=tr("Analysing your previous move…","Analyserar ditt föregående drag…");
 else if(!app.hintText.empty())text=app.hintText;
 else if(app.mode==2&&app.lanPeer&&!app.lanPeer->myTurn())text=tr("Waiting for opponent…","Väntar på motspelaren…");
 else if(app.game.board().inCheck(app.game.board().white))text=tr("Check!","Schack!");
 else text=tr("Select a piece, then a square.","Klicka på en pjäs och sedan en ruta.");
 if(!app.transient.empty())text=app.transient;
 if(app.selected>=0 && app.game.result()=="ongoing" && app.transient.empty()) {
  const auto& b=app.game.board();
  const auto moves=b.legalMoves();
  const auto n=std::count_if(moves.begin(),moves.end(),[](const chess::Move& m){return m.from==app.selected;});
  text=std::string(tr("Selected ","Vald "))+pieceName(b.squares[app.selected])+
       " "+chess::squareName(app.selected)+" · "+std::to_string(n)+
       tr(" legal moves"," möjliga drag");
 }
 g.gtk_label_set_label(app.status,text.c_str());
 if(app.boardSummary){
  const auto& board=app.game.board();
  std::string summary=(board.white?tr("White to move","Vit vid draget"):tr("Black to move","Svart vid draget"));
  if(app.game.result()!="ongoing")summary=tr("Game finished","Partiet avslutat");
  summary+=" · "+text;
  if(app.mode==2){summary+=" · ";summary+=tr("LAN game","Nätverksparti");}
  if(app.keyboardCursor)summary+=" · "+chess::squareName(app.cursor);
  g.gtk_label_set_label(app.boardSummary,summary.c_str());
  updateBoardAccessibility();
 }
 g.gtk_label_set_label(app.turn,result=="ongoing"?(app.game.board().white?tr("White to move","Vit vid draget"):tr("Black to move","Svart vid draget")):tr("Game over","Partiet avslutat"));
 std::string history;
 for(size_t k=0;k<app.game.sans.size();k++){
  if(!(k%2))history+=std::to_string(k/2+1)+". ";
  history+=app.game.sans[k];history+=(k%2)?"\n":"      ";
 }
 if(history.empty())history=tr("No moves yet","Inga drag ännu");
 if(history.size()>1800)history=history.substr(history.size()-1800);
 g.gtk_label_set_label(app.history,history.c_str());
 g.gtk_button_set_label(app.modeBtn,app.mode?tr("Mode: two players","Läge: två spelare"):tr("Mode: vs computer","Läge: mot datorn"));
 const char* skills[]={"",tr("Beginner","Nybörjare"),tr("Easy","Lätt"),tr("Medium","Medel"),tr("Hard","Svår"),tr("Challenge","Utmaning")};
 g.gtk_button_set_label(app.levelBtn,(std::string(tr("Computer: ","Dator: "))+std::to_string(app.skill)+" / 5 – "+skills[app.skill]).c_str());
 const char* paletteNames[]={tr("Nordic blue","Nordiskt blått"),tr("Light","Ljust läge"),tr("Classic wood","Klassiskt trä"),tr("High contrast","Hög kontrast")};
 g.gtk_button_set_label(app.themeBtn,(std::string(tr("Board: ","Bräde: "))+paletteNames[app.palette]).c_str());
 g.gtk_widget_set_sensitive(app.hintBtn,(!app.training&&!app.busy&&!app.thinkingHint&&app.game.result()=="ongoing" && (app.mode==1||app.game.board().white==(app.human==0)))?1:0);
 if(app.analyzeBtn)g.gtk_widget_set_sensitive(app.analyzeBtn,!app.training&&!app.analysisBusy&&!app.game.moves.empty()&&app.mode!=2);

 g.gtk_button_set_label(app.sideBtn,app.human?tr("You play: Black","Du spelar: svart"):tr("You play: White","Du spelar: vit"));
 const char* promoname=(app.promotion=='q'?tr("Queen","Dam"):app.promotion=='r'?tr("Rook","Torn"):app.promotion=='b'?tr("Bishop","Löpare"):tr("Knight","Springare"));
 g.gtk_button_set_label(app.promoteBtn,(std::string(tr("Promotion: ","Bondeförvandling: "))+promoname).c_str());
 int materialBalance=0;
 for(char pc:app.game.board().squares){
  int val=0;switch(std::tolower(static_cast<unsigned char>(pc))){case 'p':val=1;break;case 'n':case 'b':val=3;break;case 'r':val=5;break;case 'q':val=9;break;default:break;}
  if(pc)materialBalance+=(std::isupper(static_cast<unsigned char>(pc))?val:-val);
 }
 const std::string materialText=materialBalance==0?tr("Material: equal","Material: jämnt"):
    std::string(tr("Material: ","Material: "))+(materialBalance>0?tr("White","vit"):tr("Black","svart"))+" +"+std::to_string(std::abs(materialBalance));
 g.gtk_label_set_label(app.material,materialText.c_str());
 g.gtk_widget_set_visible(app.lessonBox,app.training?1:0);
 if(app.training){
  const auto& l=chess::lesson(app.lessonIndex);
  g.gtk_label_set_label(app.lessonTitle,(std::string(tr("Lesson ","Övning "))+std::to_string(app.lessonIndex+1)+" / "+std::to_string(chess::lessonCount())+" · "+tr(chess::lessonLevelEn(app.lessonIndex),chess::lessonLevelSv(app.lessonIndex))+" – "+tr(l.titleEn,l.titleSv)).c_str());
  unsigned completed=0;
  for(std::size_t i=0;i<chess::lessonCount();++i)if(app.lessonMask & (std::uint64_t(1)<<i))++completed;
  g.gtk_label_set_label(app.lessonProgress,(std::string(tr("Lessons completed: ","Avklarade övningar: "))+std::to_string(completed)+" / "+std::to_string(chess::lessonCount())+"  •  "+((app.lessonMask & (std::uint64_t(1)<<app.lessonIndex))?tr("✓ Previously completed","✓ Klar tidigare"):tr("New lesson","Ny övning"))+" · "+std::to_string(app.lessonMistakes[app.lessonIndex])+tr(" mistakes"," fel")).c_str());
  // One inexpensive profile refresh over at most 64 exercises.
  if(app.lessonProfile){
   std::string profile=tr("Knowledge profile:","Kunskapsprofil:");
   for(std::size_t group=0;group<chess::lessonGroupCount();++group){
    unsigned total=0,done=0,solved=0,errors=0;
    for(std::size_t i=0;i<chess::lessonCount();++i)if(chess::lessonGroup(i)==group){
     ++total;
     if(app.lessonMask & (std::uint64_t(1)<<i))++done;
     solved+=app.lessonSuccesses[i];errors+=app.lessonMistakes[i];
    }
    profile+="\n";
    profile+=tr(chess::lessonLevelEn(group==0?0:group==1?9:group==2?15:20),
                chess::lessonLevelSv(group==0?0:group==1?9:group==2?15:20));
    profile+=": "+std::to_string(done)+"/"+std::to_string(total);
    profile+=" · "+std::to_string(solved)+tr(" correct"," rätt");
    profile+=" · "+std::to_string(errors)+tr(" mistakes"," fel");
   }
   g.gtk_label_set_label(app.lessonProfile,profile.c_str());
  }
  std::string task=tr(l.taskEn,l.taskSv);
  const char* goalEn=chess::lessonGoalEn(app.lessonIndex);
  if(*goalEn){
   task+=" · "+std::string(tr("Goal: ","Mål: "))+
         tr(goalEn,chess::lessonGoalSv(app.lessonIndex));
  }
  const auto length=chess::lessonLineLength(app.lessonIndex);
  if(length>1&&!app.lessonSolved){
   task+=" · "+std::string(tr("Your move ","Ditt drag "))+std::to_string(app.lessonPly/2+1)+
         "/"+std::to_string((length+1)/2);
   if(app.lessonPly>0)task+=" · "+std::string(tr("Opponent replied.","Motspelaren har svarat."));
  }
  g.gtk_label_set_label(app.lessonTask,task.c_str());
  // After each opponent reply, explain that exact continuation, not ply 1
  // repeatedly when a five-ply lesson reaches the second defensive reply.
  const char* coaching=app.lessonPly>=2?
    tr(chess::lessonStepFeedbackEn(app.lessonIndex,app.lessonPly-1,app.lessonVariation),
       chess::lessonStepFeedbackSv(app.lessonIndex,app.lessonPly-1,app.lessonVariation)):
    tr("Make the move on the board. A hint is available below.",
       "Gör draget på brädet. Ledtråd finns nedan.");
  g.gtk_label_set_label(app.lessonFeedback,app.lessonSolved?tr(l.successEn,l.successSv):coaching);
 }
 repaint();
}
void reset(){if(app.mode==2){app.lanMessage=tr("Disconnect before starting a new game.","Koppla från innan du startar nytt parti.");refresh();return;}app.epoch++;app.busy=false;clearHint();app.training=false;app.lessonSolved=false;app.analysisBusy=false;stopPieceMotion();app.game.reset();
 app.flipped=app.mode==0&&app.human==1;app.selected=-1;app.transient.clear();save();refresh();}
void startAI();
void afterMove(chess::Move m,bool byComputer=false){
 if(app.game.result()!="ongoing")return;
 if(app.mode==2){
  std::string error;
  if(!app.lanPeer || !app.lanPeer->myTurn() || !app.lanPeer->sendMove(chess::uci(m),&error)){
   app.lanMessage=std::string(tr("Network move failed: ","Nätverksdrag misslyckades: "))+lanErrorText(error);
   refresh();return;
  }
 }
 const char moving=(m.from>=0&&m.from<64)?app.game.board().squares[m.from]:0;
 if(!app.game.play(m))return;
 startPieceMotion(m,moving);
 app.analysisBusy=false;
 app.selected=-1;app.keyboardCursor=false;app.transient.clear();clearHint();app.epoch++;
 if(app.training){
  ++app.lessonPly;
  // Play only the predetermined legal opponent reply, never an engine
  // move or network action. The player must still make the final move.
  if(app.lessonPly<chess::lessonLineLength(app.lessonIndex) &&
     (app.lessonPly%2)==1) {
   const char* reply=chess::lessonLineMoveVariant(app.lessonIndex,app.lessonPly,
                                                          app.lessonVariation);
   // The board has already advanced to Black's reply, so do not keep
   // animating a now-outdated white-only snapshot.
   stopPieceMotion();
   if(reply && app.game.playUci(reply)){
    app.transient=tr(chess::lessonStepFeedbackEn(app.lessonIndex,app.lessonPly,app.lessonVariation),
                     chess::lessonStepFeedbackSv(app.lessonIndex,app.lessonPly,app.lessonVariation));
    ++app.lessonPly;
   }
   else {
    app.transient=tr("Lesson reply could not be played. Restart this exercise.",
                     "Övningens svarsdrag kunde inte spelas. Starta om övningen.");
   }
  }
  if(app.lessonPly==chess::lessonLineLength(app.lessonIndex)){
   app.lessonSolved=true;
   app.lessonMask|=(std::uint64_t(1)<<app.lessonIndex);
   if(app.lessonSuccesses[app.lessonIndex]<999)++app.lessonSuccesses[app.lessonIndex];
   saveProgress();
  }
 }else save();
 schack_system::playMoveSound(app.game.result()!="ongoing");
 refresh();if(!byComputer&&!app.training)startAI();
}
const char* explanation(chess::MoveReason reason){
 switch(reason){
 case chess::MoveReason::Checkmate:return tr("This move delivers checkmate.","Draget ger schack matt.");
 case chess::MoveReason::Check:return tr("This move gives check.","Draget ger schack.");
 case chess::MoveReason::Promotion:return tr("A pawn becomes a stronger piece.","En bonde förvandlas till en starkare pjäs.");
 case chess::MoveReason::Castle:return tr("Castling helps protect the king.","Rockad hjälper till att skydda kungen.");
 case chess::MoveReason::Capture:return tr("This move captures an opponent's piece.","Draget slår en av motståndarens pjäser.");
 case chess::MoveReason::Develop:return tr("Developing a piece brings it into the game.","Att utveckla en pjäs får den i spel.");
 case chess::MoveReason::Center:return tr("This move occupies a central square.","Draget tar en ruta i centrum.");
 default:return tr("A legal suggestion from the computer; consider your opponent's reply.",
                   "Ett lagligt förslag från datorn. Fundera på motståndarens svar.");
 }
}
struct AIResult{unsigned epoch; chess::SearchResult result; bool hint=false;};
int deliverAI(void* data){
 std::unique_ptr<AIResult> r(static_cast<AIResult*>(data));
 if(app.closed)return 0;
 if(r->epoch==app.epoch){
  if(r->hint){
   app.thinkingHint=false;
   if(r->result.move.from>=0){
    app.hintFrom=r->result.move.from;app.hintTo=r->result.move.to;
    app.hintText=std::string(tr("Suggestion: ","Förslag: "))+chess::san(app.game.board(),r->result.move)
      +" ("+chess::squareName(app.hintFrom)+" → "+chess::squareName(app.hintTo)+") · "+
      explanation(chess::explainMove(app.game.board(),r->result.move));
   }
   refresh();return 0;
  }
  app.busy=false;
  if(!app.training&&app.mode==0&&app.game.result()=="ongoing"&&app.game.board().white!=(app.human==0))
   afterMove(r->result.move,true);
  refresh();
 }
 return 0;
}
void startAI(){
 if(app.closed||app.training||app.mode!=0||app.busy||app.game.result()!="ongoing")return;
 if(app.game.board().white==(app.human==0))return;
 const chess::Board board=app.game.board(); const int skill=app.skill;const unsigned epoch=app.epoch;
 app.busy=true;refresh();
 std::thread([board,skill,epoch](){
  auto* r=new AIResult{epoch,chess::bestMove(board,skill,skill==5?120000:50000),false};
  g_idle_add(deliverAI,r);
 }).detach();
}
void suggestMove(){
 if(app.closed||app.training||app.busy||app.thinkingHint||app.game.result()!="ongoing")return;
 if(app.mode==0 && app.game.board().white!=(app.human==0))return;
 const auto b=app.game.board();const unsigned epoch=app.epoch;
 app.hintFrom=app.hintTo=-1;app.hintText.clear();app.thinkingHint=true;refresh();
 std::thread([b,epoch](){
  auto* result=new AIResult{epoch,chess::bestMove(b,3,18000),true};
  g_idle_add(deliverAI,result);
 }).detach();
}
// An explicit user request analyses the previous position with a small node
// budget. Never blocks GTK or labels an unsearched move as a proven blunder.
struct MoveReview {
 unsigned epoch;
 chess::Board before;
 chess::Move played;
 chess::SearchResult suggested;
};
int deliverMoveReview(void* data){
 std::unique_ptr<MoveReview> r(static_cast<MoveReview*>(data));
 if(app.closed||r->epoch!=app.epoch)return 0;
 app.analysisBusy=false;
 const auto actual=chess::san(r->before,r->played);
 std::string message=std::string(tr("Your move: ","Ditt drag: "))+actual+
     " · "+explanation(chess::explainMove(r->before,r->played));
 if(r->suggested.move.from>=0){
  if(r->played==r->suggested.move)
   message+=" · "+std::string(tr("Also chosen by the limited computer analysis.",
                          "Valdes även av datorns begränsade analys."));
  else message+=" · "+std::string(tr("Another candidate: ","Ett annat alternativ: "))+
       chess::san(r->before,r->suggested.move)+
       tr(" (not a proven improvement)."," (inte bevisat bättre).");
 }
 app.transient=message;refresh();return 0;
}
void analyzeLastMove(){
 if(app.closed||app.training||app.mode==2||app.analysisBusy||app.game.moves.empty())return;
 const auto before=app.game.positions[app.game.positions.size()-2];
 const auto played=app.game.moves.back();
 const unsigned epoch=app.epoch;
 app.analysisBusy=true;app.transient.clear();refresh();
 std::thread([before,played,epoch](){
  auto* r=new MoveReview{epoch,before,played,chess::bestMove(before,3,8000)};
  g_idle_add(deliverMoveReview,r);
 }).detach();
}
void chosen(int sq){
 if(sq<0||app.game.result()!="ongoing")return;
 if(app.training&&app.lessonSolved)return;
 if(!app.training&&app.mode==0&&(app.busy||app.game.board().white!=(app.human==0)))return;
 if(app.mode==2&&(!app.lanPeer||!app.lanPeer->myTurn()))return;
 const auto board=app.game.board();
 if(app.selected>=0&&app.selected!=sq){
  for(auto m:board.legalMoves()){
   if(m.from!=app.selected||m.to!=sq)continue;
   if(m.promotion)m.promotion=app.promotion;
   if(app.training&&!chess::lessonAcceptsMove(app.lessonIndex,app.lessonPly,app.lessonVariation,board,m)){
    if(app.lessonMistakes[app.lessonIndex]<999)++app.lessonMistakes[app.lessonIndex];
    saveProgress();
    app.transient=app.lessonMistakes[app.lessonIndex]>=2?
      tr(chess::lessonStepHintEn(app.lessonIndex,app.lessonPly,app.lessonVariation),
         chess::lessonStepHintSv(app.lessonIndex,app.lessonPly,app.lessonVariation)):
      tr("Not quite. Try again: check the squares your piece controls, or use the hint.",
         "Inte riktigt. Försök igen: se vilka rutor pjäsen kontrollerar, eller använd ledtråden.");
    refresh();return;
   }
   afterMove(m);return;
  }
 }
 const char p=board.squares[sq];
 app.selected=(p&&((std::isupper(static_cast<unsigned char>(p))!=0)==board.white))?sq:-1;
 app.cursor=sq;app.transient.clear();if(app.hintFrom>=0)clearHint();refresh();
}
void draw(Ptr,cairo_t* c,int width,int height,void*){
 if(app.closed)return;
 const auto geo=boardGeom(width,height); const auto& b=app.game.board();
 const auto& pal=colors();
 rgba(c,pal.bg[0],pal.bg[1],pal.bg[2]);cairo_rectangle(c,0,0,width,height);cairo_fill(c);
 rgba(c,pal.edge[0],pal.edge[1],pal.edge[2]);cairo_rectangle(c,geo.x-4,geo.y-4,geo.s*8+8,geo.s*8+8);cairo_fill(c);
 std::vector<chess::Move> allowed;
 if(app.selected>=0)allowed=b.legalMoves();
 int checked=-1;if(b.inCheck(b.white))for(int sq=0;sq<64;sq++)if(b.squares[sq]==(b.white?'K':'k')){checked=sq;break;}
 for(int row=0;row<8;row++)for(int file=0;file<8;file++){
  const int sq=(app.flipped?row:7-row)*8+(app.flipped?7-file:file);
  const double x=geo.x+file*geo.s,y=geo.y+row*geo.s;
  const double* baseColor=(row+file)%2==0?pal.light:pal.dark;
  rgba(c,baseColor[0],baseColor[1],baseColor[2]);
  cairo_rectangle(c,x,y,geo.s,geo.s);cairo_fill(c);
  if(!app.game.moves.empty()&&(sq==app.game.moves.back().from||sq==app.game.moves.back().to)){
   rgba(c,1,.75,.22,.3);cairo_rectangle(c,x,y,geo.s,geo.s);cairo_fill(c);
  }
  if(sq==checked){rgba(c,.89,.22,.23,.37);cairo_rectangle(c,x,y,geo.s,geo.s);cairo_fill(c);}
  if(sq==app.selected){
   rgba(c,pal.accent[0],pal.accent[1],pal.accent[2],.35);
   cairo_rectangle(c,x,y,geo.s,geo.s);cairo_fill(c);
   // Outline, not just a colour overlay: legible on the light, dark and high-contrast themes.
   rgba(c,pal.accent[0],pal.accent[1],pal.accent[2],1);
   cairo_set_line_width(c,std::clamp(geo.s*.055,2.5,5.0));
   cairo_rectangle(c,x+3,y+3,geo.s-6,geo.s-6);cairo_stroke(c);
  }
  if(std::any_of(allowed.begin(),allowed.end(),[&](const chess::Move& m){return m.from==app.selected&&m.to==sq;})){
   rgba(c,.12,.27,.42,.47);cairo_new_path(c);
   if(b.squares[sq]){cairo_set_line_width(c,std::max(2.,geo.s*.075));cairo_arc(c,x+geo.s/2,y+geo.s/2,geo.s*.44,0,6.28318530718);cairo_stroke(c);}
   else{cairo_arc(c,x+geo.s/2,y+geo.s/2,geo.s*.12,0,6.28318530718);cairo_fill(c);}
  }
  if(sq==app.hintFrom||sq==app.hintTo){
   rgba(c,.08,.82,.72,.92);cairo_set_line_width(c,std::max(2.5,geo.s*.075));
   cairo_rectangle(c,x+geo.s*.06,y+geo.s*.06,geo.s*.88,geo.s*.88);cairo_stroke(c);
  }
  // While a move animates, paint its destination only once at interpolated coordinates.
  if(!(app.movingPiece&&sq==app.motionTo))piece(c,b.squares[sq],x,y,geo.s);
  if(app.keyboardCursor && sq==app.cursor){
   rgba(c,pal.accent[0],pal.accent[1],pal.accent[2],1);
   cairo_set_line_width(c,3);
   cairo_rectangle(c,x+2,y+2,geo.s-4,geo.s-4);cairo_stroke(c);
   cairo_rectangle(c,x+7,y+7,geo.s-14,geo.s-14);cairo_stroke(c);
  }
 }
 if(app.movingPiece&&app.motionFrom>=0&&app.motionTo>=0){
  constexpr double totalMs=160.;
  const double elapsed=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-app.motionStarted).count();
  const double t=std::clamp(elapsed/totalMs,0.,1.);
  const auto xy=[&](int sq){
   const int f=sq%8,r=sq/8;
   return std::pair<double,double>{geo.x+(app.flipped?7-f:f)*geo.s,
       geo.y+(app.flipped?r:7-r)*geo.s};
  };
  const auto a=xy(app.motionFrom),z=xy(app.motionTo);
  piece(c,app.motionGlyph,a.first+(z.first-a.first)*t,
        a.second+(z.second-a.second)*t,geo.s);
 }
 rgba(c,app.palette==1?.10:.90,app.palette==1?.24:.94,app.palette==1?.36:1.);cairo_select_font_face(c,"Sans",0,0);cairo_set_font_size(c,std::clamp(geo.s*.19,8.,13.));
 for(int i=0;i<8;i++){
  const char row[]={char('1'+(app.flipped?i:7-i)),0};
  const char col[]={char('a'+(app.flipped?7-i:i)),0};
  cairo_move_to(c,geo.x-17,geo.y+(i+.56)*geo.s);cairo_show_text(c,row);
  cairo_move_to(c,geo.x+(i+.44)*geo.s,geo.y+8*geo.s+15);cairo_show_text(c,col);
 }
}
int boardWidth=720,boardHeight=680;
int hitSquare(double x,double y){
 const auto geo=boardGeom(boardWidth,boardHeight);
 const int file=int(std::floor((x-geo.x)/geo.s)),row=int(std::floor((y-geo.y)/geo.s));
 if(file<0||file>7||row<0||row>7)return -1;
 return (app.flipped?row:7-row)*8+(app.flipped?7-file:file);
}
void onDragBegin(Ptr,double x,double y,void*){
 app.dragX=x;app.dragY=y;app.dragOrigin=hitSquare(x,y);
}
void onDragEnd(Ptr,double offsetX,double offsetY,void*){
 if(app.dragOrigin<0)return;
 const int target=hitSquare(app.dragX+offsetX,app.dragY+offsetY);
 if(target>=0&&target!=app.dragOrigin){
  app.keyboardCursor=false;
  if(app.selected!=app.dragOrigin)chosen(app.dragOrigin);
  chosen(target);
 }
 app.dragOrigin=-1;
}
void onClick(Ptr,int n,double x,double y,void*){
 if(n!=1||app.closed)return;
 // GTK reports coordinates local to the drawing area.
 int sq=hitSquare(x,y);if(sq<0)return;
 app.keyboardCursor=false;chosen(sq);
 g.gtk_widget_grab_focus(app.board);
}
void drawRemember(Ptr p,cairo_t* c,int w,int h,void* d){boardWidth=w;boardHeight=h;draw(p,c,w,h,d);}
void perform(int id);
void startLan(bool host);
void stopLan();
int pollLan(void*);
int onKey(Ptr,unsigned key,unsigned,unsigned state,void*){
 // Text entry must retain arrows, Space and shortcuts while typing LAN details.
 if((app.lanEntry&&g.gtk_widget_has_focus(app.lanEntry)) ||
    (app.lanCodeEntry&&g.gtk_widget_has_focus(app.lanCodeEntry)))return 0;
 // Arrow keys and Enter move pieces only when the board has focus.
 // Global Ctrl shortcuts remain available from ordinary buttons.
 if(!(state&(1u<<2)) && (!app.board || !g.gtk_widget_has_focus(app.board)))return 0;
 if(state&(1u<<2)) { // Ctrl shortcuts
  unsigned keyLower=(key>='A'&&key<='Z')?key+32:key;
  if(keyLower=='n'){perform(0);return 1;}
  if(keyLower=='z'){perform(1);return 1;}
  if(keyLower=='s'){perform(10);return 1;}
  if(keyLower=='r'){perform(2);return 1;}
  if(keyLower=='h'){perform(12);return 1;}
  if(keyLower=='l'){perform(14);return 1;}
  return 0;
 }
 if(state&(1u<<3))return 0; // Alt
 const int f=app.cursor%8,r=app.cursor/8;int x=f,y=r,d=app.flipped?-1:1;
 if(key==0xff51)x-=d;else if(key==0xff53)x+=d;
 else if(key==0xff52)y+=d;else if(key==0xff54)y-=d;
 else if(key==0xff0d||key==0x20){app.keyboardCursor=true;chosen(app.cursor);return 1;}
 else if(key==0xff1b){app.selected=-1;refresh();return 1;}
 else return 0;
 app.keyboardCursor=true;
 app.cursor=std::clamp(y,0,7)*8+std::clamp(x,0,7);
 refresh();return 1;
}
void lessonLoad(){
 app.epoch++;app.busy=false;app.analysisBusy=false;clearHint();app.lessonSolved=false;app.lessonPly=0;
 app.lessonVariation=app.lessonSuccesses[app.lessonIndex]%
                      chess::lessonVariationCount(app.lessonIndex);
 app.selected=-1;app.transient.clear();
 stopPieceMotion();app.analysisBusy=false;app.game.loadFen(chess::lesson(app.lessonIndex).fen);refresh();
}
// The only network entry points are the two visible Host/Join buttons.
std::string localLanAddress(){
 ifaddrs* interfaces=nullptr;
 if(::getifaddrs(&interfaces)!=0)return tr("See network settings","Se nätverksinställningar");
 std::string address;
 for(auto* a=interfaces;a;a=a->ifa_next){
  if(!a->ifa_addr||a->ifa_addr->sa_family!=AF_INET)continue;
  char text[INET_ADDRSTRLEN]{};
  ::inet_ntop(AF_INET,&reinterpret_cast<sockaddr_in*>(a->ifa_addr)->sin_addr,text,sizeof(text));
  if(chess::lan::localIPv4(text)&&std::string(text).rfind("127.",0)!=0){
   address=text;break;
  }
 }
 ::freeifaddrs(interfaces);
 return address.empty()?tr("See network settings","Se nätverksinställningar"):address;
}
struct LanResult {
 unsigned epoch;
 bool host=false;
 std::shared_ptr<chess::lan::Peer> peer;
 std::shared_ptr<std::atomic<bool>> cancel;
 std::string error;
};
int deliverLan(void* data){
 std::unique_ptr<LanResult> r(static_cast<LanResult*>(data));
 if(app.closed||r->cancel->load()||r->epoch!=app.epoch)return 0;
 app.lanConnecting=false;
 if(!r->peer){
  app.lanMessage=std::string(tr("LAN connection failed: ","Nätverksanslutningen misslyckades: "))+lanErrorText(r->error);
  refresh();return 0;
 }
 app.lanPeer=std::move(r->peer);
 app.mode=2;app.human=r->host?0:1;app.flipped=!r->host;
 app.training=false;app.lessonSolved=false;
 app.selected=-1;app.keyboardCursor=false;app.epoch++;
 clearHint();stopPieceMotion();app.game.reset();
 app.lanMessage=r->host?tr("LAN game connected. You play White.","LAN-anslutning upprättad. Du spelar vit."):
                            tr("LAN game connected. You play Black.","LAN-anslutning upprättad. Du spelar svart.");
 refresh();return 0;
}
std::string freshPairingCode(){
 std::uint32_t randomValue=0;
 if(::getrandom(&randomValue,sizeof(randomValue),0)!=sizeof(randomValue))return {};
 std::ostringstream out;
 out<<std::setw(6)<<std::setfill('0')<<(randomValue%1000000U);
 return out.str();
}
const char* lanErrorText(const std::string& error) {
 struct Translation {const char* en;const char* sv;};
 static const Translation translations[]={
  {"Pairing code rejected","Parningskoden godkändes inte"},
  {"Pairing code rejected or peer disconnected","Parningskoden godkändes inte eller motspelaren kopplade från"},
  {"Protocol handshake failed","Datorerna använder olika nätverksprotokoll eller anslutningen avbröts"},
  {"Invalid pairing request","Ogiltig parningsförfrågan"},
  {"Six-digit pairing code required","En sexsiffrig parningskod behövs"},
  {"Invalid port, timeout or pairing code","Ogiltig port, tidsgräns eller parningskod"},
  {"Use a private IPv4 network address","Ange en privat IPv4-adress på det lokala nätverket"},
  {"Could not listen on port","Det gick inte att starta nätverksvärden"},
  {"Cannot create listener","Det gick inte att öppna en lyssnande nätverksanslutning"},
  {"Waiting for a player timed out or cancelled","Väntan på motspelare avbröts eller tog för lång tid"},
  {"Connection timed out","Anslutningen tog för lång tid"},
  {"Connection failed","Anslutningen misslyckades"},
  {"Peer unavailable","Motspelaren gick inte att nå"},
  {"Cannot create socket","Nätverksanslutningen kunde inte öppnas"},
  {"Only private IPv4 peers are allowed","Bara datorer med privata IPv4-adresser tillåts"},
  {"No move received or peer disconnected","Inget drag togs emot eller motspelaren kopplade från"},
  {"Peer disconnected","Motspelaren kopplade från"},
  {"Connection lost","Anslutningen bröts"},
  {"Not your turn","Det är inte din tur"},
  {"Illegal move","Otillåtet drag"},
  {"Invalid move notation","Ogiltig dragnotation"},
  {"Remote player sent an illegal move","Motspelaren skickade ett otillåtet drag"},
  {"Unexpected network message","Oväntat nätverksmeddelande"},
  {"Network message too long","Nätverksmeddelandet är för långt"},
  {"Cancelled","Avbrutet"}
 };
 for(const auto& t:translations)if(error==t.en)return tr(t.en,t.sv);
 return tr("Network connection error","Fel i nätverksanslutningen");
}
void startLan(bool host){
 if(app.closed||app.lanConnecting||app.lanPeer)return;
 std::string address;
 std::string code;
 if(host){
  code=freshPairingCode();
  if(code.empty()){
   app.lanMessage=tr("Could not generate a pairing code.","Det gick inte att skapa en parningskod.");
   refresh();return;
  }
 }else{
  const char* typed=g.gtk_editable_get_text(app.lanCodeEntry);
  code=typed?typed:"";
  if(!chess::lan::validPairingCode(code)){
   app.lanMessage=tr("Enter the six-digit pairing code shown by the host.","Ange den sexsiffriga parningskoden som värden visar.");
   refresh();return;
  }
  g.gtk_editable_set_text(app.lanCodeEntry,"");
  const char* ip=g.gtk_editable_get_text(app.lanEntry);
  address=ip?ip:"";
  if(!chess::lan::localIPv4(address)){
   app.lanMessage=tr("Enter the other computer's private IPv4 address.","Ange den andra datorns privata IPv4-adress.");
   refresh();return;
  }
 }
 auto cancel=std::make_shared<std::atomic<bool>>(false);
 app.analysisBusy=false;app.lanCancel=cancel;app.lanConnecting=true;
 const unsigned epoch=++app.epoch;
 if(host)app.lanMessage=std::string(tr("Waiting for opponent at ","Väntar på motspelare på "))+localLanAddress()+":47777"+
  tr(" · Pairing code: "," · Parningskod: ")+code;
 else app.lanMessage=tr("Connecting to the local computer…","Ansluter till den lokala datorn…");
 refresh();
 std::thread([host,address,code,epoch,cancel]{
  auto peer=std::make_shared<chess::lan::Peer>();
  std::string error;
  const bool ok=host?peer->host(chess::lan::defaultPort,60000,&error,cancel.get(),code):
                    peer->join(address,chess::lan::defaultPort,5000,&error,code);
  if(cancel->load())return;
  auto* result=new LanResult{epoch,host,ok?peer:nullptr,cancel,error};
  g_idle_add(deliverLan,result);
 }).detach();
}
void stopLan(){
 if(app.lanCancel)app.lanCancel->store(true);
 if(app.lanPeer)app.lanPeer->close();
 app.lanPeer.reset();app.lanConnecting=false;app.analysisBusy=false;app.epoch++;
 if(app.mode==2){app.mode=1;app.human=0;}
 app.lanMessage=tr("Disconnected. The board remains available for local play.",
                   "Frånkopplad. Brädet går att fortsätta spela på lokalt.");
 refresh();
}
int pollLan(void*){
 if(app.closed)return 0;
 if(!app.lanPeer||app.mode!=2)return 1;
 std::string move,error;
 const auto state=app.lanPeer->pollMove(move,&error);
 if(state==chess::lan::Peer::Incoming::Move){
  // The transport already validated the move; repeat validation on the UI game.
  const auto before=app.game.board();
  const auto candidates=before.legalMoves();
  const auto match=std::find_if(candidates.begin(),candidates.end(),
      [&](const chess::Move& candidate){return chess::uci(candidate)==move;});
  if(!app.game.playUci(move)){
   app.lanMessage=tr("The boards differ. Connection closed.","Brädena skiljer sig åt. Anslutningen har stängts.");
   app.lanPeer->close();app.lanPeer.reset();app.mode=1;
  }else{
   if(match!=candidates.end())startPieceMotion(*match,before.squares[match->from]);
   app.selected=-1;app.keyboardCursor=false;clearHint();app.epoch++;
   app.lanMessage=std::string(tr("Opponent played ","Motspelaren spelade "))+move;
   schack_system::playMoveSound(app.game.result()!="ongoing");
  }
  refresh();
 }else if(state==chess::lan::Peer::Incoming::Closed){
  app.lanPeer.reset();app.mode=1;app.lanConnecting=false;app.epoch++;
  app.lanMessage=tr("Opponent disconnected. Continue locally or start a new LAN game.",
                    "Motspelaren kopplade från. Fortsätt lokalt eller starta nytt nätverksparti.");
  refresh();
 }
 return 1;
}

void perform(int id){
 if(app.closed)return;
 switch(id){
 case 0:reset();break;
 case 1: // Undo (normal play) or restart the entire scripted exercise
  if(app.mode==2){app.lanMessage=tr("Undo is unavailable in LAN games.","Ångra är inte tillgängligt i nätverkspartier.");refresh();break;}
  if(app.training){
   // Undoing a single ply would leave the scripted lessonPly out of sync.
   lessonLoad();
   app.transient=tr("Exercise restarted. Follow the plan from the beginning.",
                    "Övningen har startats om. Följ planen från början.");
   refresh();break;
  }
  if(app.game.undo()){
   stopPieceMotion();app.epoch++;app.busy=false;clearHint();app.lessonSolved=false;
   if(!app.training&&app.mode==0&&!app.game.moves.empty()&&app.game.board().white!=(app.human==0))app.game.undo();
   app.selected=-1;app.transient.clear();save();refresh();startAI();
  }break;
 case 2:stopPieceMotion();app.flipped=!app.flipped;app.transient.clear();save();refresh();break;
 case 3:if(app.mode==2){app.lanMessage=tr("Disconnect from LAN first.","Koppla från nätverkspartiet först.");refresh();break;}app.mode=1-app.mode;reset();startAI();break;
 case 4:app.skill=1+(app.skill%5);app.transient.clear();save();refresh();break;
 case 5:if(app.mode==2){app.lanMessage=tr("Sides cannot change in a LAN game.","Sidorna kan inte bytas i nätverkspartier.");refresh();break;}app.human=1-app.human;reset();startAI();break;
 case 6: // Schackskola toggle
  if(app.mode==2){app.lanMessage=tr("Disconnect before opening lessons.","Koppla från innan du öppnar schackskolan.");refresh();break;}
  if(!app.training){
   save();app.savedGame=app.game;app.training=true;
   app.lessonIndex=chess::recommendedLesson(app.lessonMask,app.lessonMistakes,app.lessonSuccesses);lessonLoad();
  }else{
   stopPieceMotion();app.analysisBusy=false;app.training=false;app.lessonSolved=false;app.game=app.savedGame;
   app.epoch++;app.busy=false;clearHint();app.selected=-1;app.transient.clear();refresh();startAI();
  }break;
 case 7:if(app.training&&app.lessonIndex>0){app.lessonIndex--;lessonLoad();}break;
 case 8:if(app.training){
  app.transient=tr(chess::lessonStepHintEn(app.lessonIndex,app.lessonPly,app.lessonVariation),
                   chess::lessonStepHintSv(app.lessonIndex,app.lessonPly,app.lessonVariation));
  refresh();
 }break;
 case 9:if(app.training&&app.lessonIndex+1<chess::lessonCount()){app.lessonIndex++;lessonLoad();}break;
 case 19:if(app.training){app.lessonIndex=chess::recommendedLesson(app.lessonMask,app.lessonMistakes,app.lessonSuccesses);lessonLoad();}break;
 case 10: { // Export PGN
  try{
   std::filesystem::create_directories(userDir());
   std::time_t now=std::time(nullptr);std::tm local{};
   #if defined(_WIN32)
   localtime_s(&local,&now);
   #else
   localtime_r(&now,&local);
   #endif
   char stamp[32];std::strftime(stamp,sizeof(stamp),"%Y%m%d-%H%M%S",&local);
   const std::string filepath=userDir()+"/parti-"+stamp+".pgn";
   std::ofstream o(filepath);o<<app.game.pgn();o.close();
   if(!o)throw std::runtime_error("write failed");
   app.transient=std::string(tr("PGN saved: ","PGN sparad: "))+filepath;
  }catch(...){app.transient=tr("Could not save PGN.","Kunde inte spara PGN.");}
  refresh();break;
 }
 case 12:suggestMove();break;
 case 18:analyzeLastMove();break;
 case 13:app.palette=(app.palette+1)%4;theme();save();refresh();break;
 case 14:app.focusBoard=!app.focusBoard;refresh();g.gtk_widget_grab_focus(app.board);break;
 case 15:startLan(true);break;
 case 16:startLan(false);break;
 case 17:stopLan();break;
 case 11: {
  const std::string choices="qrnb";auto pos=choices.find(app.promotion);
  app.promotion=choices[(pos+1)%choices.size()];refresh();break;
 }
 default:break;
 }
}
struct ButtonData{int id;};
std::vector<std::unique_ptr<ButtonData>> buttonDatas;
void onButton(Ptr,void* data){perform(static_cast<ButtonData*>(data)->id);}
Ptr button(Ptr container,const char* label,int id,bool primary=false){
 Ptr btn=g.gtk_button_new_with_label(label);
 if(primary)g.gtk_widget_add_css_class(btn,"suggested-action");
 g.gtk_widget_add_css_class(btn,"wenuy-control");
 buttonDatas.push_back(std::make_unique<ButtonData>(ButtonData{id}));
 connect(btn,"clicked",onButton,buttonDatas.back().get());
 g.gtk_box_append(container,btn);return btn;
}
Ptr label(Ptr box,const char* text,bool wrap=false,const char* cls=nullptr){
 Ptr l=g.gtk_label_new(text);g.gtk_label_set_xalign(l,0);g.gtk_label_set_yalign(l,0);
 if(wrap)g.gtk_label_set_wrap(l,1);
 if(cls)g.gtk_widget_add_css_class(l,cls);
 g.gtk_box_append(box,l);return l;
}
void margin(Ptr w,int x,int y){
 g.gtk_widget_set_margin_start(w,x);g.gtk_widget_set_margin_end(w,x);
 g.gtk_widget_set_margin_top(w,y);g.gtk_widget_set_margin_bottom(w,y);
}
int onClose(Ptr,void*){stopPieceMotion();app.closed=true;app.epoch++;app.cssProvider=nullptr;
 if(app.lanCancel)app.lanCancel->store(true);
 if(app.lanPeer)app.lanPeer->close();
 app.lanPeer.reset();save();return 0;}
void activate(Ptr application,void*){
 if(app.window&&!app.closed){g.gtk_window_present(app.window);return;}
 app.closed=false;app.analysisBusy=false;clearHint();
 if(app.lanMessage.empty())app.lanMessage=tr("LAN is off. No network activity.","LAN är avstängt. Ingen nätverkstrafik.");
 app.window=g.gtk_application_window_new(application);
 g.gtk_window_set_title(app.window,"Wenuy Schack");g.gtk_window_set_default_size(app.window,960,700);
 connect(app.window,"close-request",onClose,nullptr);
 Ptr root=g.gtk_box_new(1,0);g.gtk_window_set_child(app.window,root);
 Ptr header=g.gtk_box_new(0,8);margin(header,10,8);
 g.gtk_widget_add_css_class(header,"wenuy-topbar");g.gtk_box_append(root,header);
 app.brand=g.gtk_label_new("♞  WENUY SCHACK");Ptr brand=app.brand;g.gtk_widget_add_css_class(brand,"title-2");
 g.gtk_widget_add_css_class(brand,"wenuy-brand");
 g.gtk_widget_set_hexpand(brand,1);g.gtk_label_set_xalign(brand,0);g.gtk_box_append(header,brand);
 button(header,tr("New game","Nytt parti"),0,true);app.undoBtn=button(header,tr("Undo","Ångra"),1);button(header,tr("Flip board","Vänd brädet"),2);
 // A separate compact toolbar keeps the game controls legible on smaller screens.
 app.toolBar=g.gtk_box_new(0,5);Ptr tools=app.toolBar;margin(tools,10,4);g.gtk_box_append(root,tools);
 button(tools,tr("Chess school","Schackskolan"),6);
 app.hintBtn=button(tools,tr("Suggest move","Visa dragförslag"),12);
 app.analyzeBtn=button(tools,tr("Review last move","Analysera senaste draget"),18);
 app.focusBtn=button(tools,tr("Focus on board","Fokus på brädet"),14);
 app.mainBox=g.gtk_box_new(0,0);Ptr main=app.mainBox;g.gtk_widget_set_vexpand(main,1);g.gtk_box_append(root,main);
 Ptr boardBox=g.gtk_box_new(1,0);g.gtk_widget_set_hexpand(boardBox,1);g.gtk_widget_set_vexpand(boardBox,1);
 g.gtk_box_append(main,boardBox);
 app.board=g.gtk_drawing_area_new();g.gtk_widget_set_hexpand(app.board,1);g.gtk_widget_set_vexpand(app.board,1);
 g.gtk_widget_set_size_request(app.board,260,260);g.gtk_widget_set_focusable(app.board,1);
 g.gtk_widget_set_tooltip_text(app.board,tr("Click two squares or drag a piece. Arrow keys and Enter also work.","Klicka på två rutor eller dra pjäsen. Piltangenter och Enter fungerar också."));
 g.gtk_drawing_area_set_draw_func(app.board,drawRemember,nullptr,nullptr);
 g.gtk_accessible_update_property(app.board,1,
   tr("Use arrow keys to navigate, Enter to select or move, Escape to cancel.",
      "Använd piltangenterna för att navigera, Enter för att välja eller flytta, Escape för att avbryta."),-1); // DESCRIPTION
 Ptr click=g.gtk_gesture_click_new();g.gtk_gesture_single_set_button(click,1);
 connect(click,"pressed",onClick,nullptr);g.gtk_widget_add_controller(app.board,click);
 Ptr drag=g.gtk_gesture_drag_new();g.gtk_gesture_single_set_button(drag,1);
 connect(drag,"drag-begin",onDragBegin,nullptr);connect(drag,"drag-end",onDragEnd,nullptr);
 g.gtk_widget_add_controller(app.board,drag);
 Ptr keys=g.gtk_event_controller_key_new();connect(keys,"key-pressed",onKey,nullptr);
 g.gtk_widget_add_controller(app.board,keys);g.gtk_box_append(boardBox,app.board);
 // A persistent text equivalent of the custom-drawn board, also visible in focus mode.
 app.boardSummary=g.gtk_label_new("");
 g.gtk_label_set_wrap(app.boardSummary,1);
 g.gtk_label_set_xalign(app.boardSummary,0);
 g.gtk_widget_add_css_class(app.boardSummary,"wenuy-dim");
 g.gtk_widget_set_margin_start(app.boardSummary,12);
 g.gtk_widget_set_margin_end(app.boardSummary,12);
 g.gtk_box_append(boardBox,app.boardSummary);
 // Window-level shortcuts also work when a sidebar button has keyboard focus.
 Ptr windowKeys=g.gtk_event_controller_key_new();connect(windowKeys,"key-pressed",onKey,nullptr);
 g.gtk_widget_add_controller(app.window,windowKeys);
 // Scroll all secondary settings: nothing is clipped at a 600-pixel screen height.
 app.sidePanel=g.gtk_scrolled_window_new();
 g.gtk_widget_set_vexpand(app.sidePanel,1);
 g.gtk_widget_set_size_request(app.sidePanel,252,-1);
 g.gtk_box_append(main,app.sidePanel);
 Ptr side=g.gtk_box_new(1,9);
 margin(side,12,8);g.gtk_widget_add_css_class(side,"wenuy-panel");
 g.gtk_scrolled_window_set_child(app.sidePanel,side);
 label(side,tr("YOUR GAME","DITT PARTI"),false,"title-3");
 app.turn=label(side,"",false,"heading");
 app.status=label(side,"",true);
 app.material=label(side,"",false,"wenuy-dim");
 label(side,tr("LAN GAME","NÄTVERKSPARTI"),false,"heading");
 label(side,tr("Trusted LAN only. Pairing code is unencrypted. Never open a router port.","Endast betrott LAN. Parningskoden är okrypterad. Öppna aldrig en port i routern."),true,"wenuy-dim");
 app.lanEntry=g.gtk_entry_new();
 g.gtk_entry_set_placeholder_text(app.lanEntry,tr("Host IPv4: 192.168.1.20","Värdens IPv4: 192.168.1.20"));
 g.gtk_accessible_update_property(app.lanEntry,4,tr("Host IPv4 address","Värdens IPv4-adress"),-1);
 g.gtk_box_append(side,app.lanEntry);
 app.lanCodeEntry=g.gtk_entry_new();
 g.gtk_entry_set_placeholder_text(app.lanCodeEntry,tr("Six-digit pairing code","Sexsiffrig parningskod"));
 g.gtk_accessible_update_property(app.lanCodeEntry,4,tr("Six-digit pairing code","Sexsiffrig parningskod"),-1);
 g.gtk_box_append(side,app.lanCodeEntry);
 app.lanHostBtn=button(side,tr("Host LAN game (White)","Bjud in på LAN (vit)"),15);
 app.lanJoinBtn=button(side,tr("Join LAN game (Black)","Anslut till LAN (svart)"),16);
 app.lanStopBtn=button(side,tr("Disconnect / Cancel","Koppla från / Avbryt"),17);
 app.lanStatus=label(side,tr("LAN is off. No network activity.","LAN är avstängt. Ingen nätverkstrafik."),true,"wenuy-dim");
 app.modeBtn=button(side,tr("Mode: vs computer","Läge: mot datorn"),3);
 app.levelBtn=button(side,tr("Level: 2 / 5","Nivå: 2 / 5"),4);
 app.sideBtn=button(side,tr("You play: White","Du spelar: vit"),5);
 app.promoteBtn=button(side,tr("Promotion: Queen","Bondeförvandling: Dam"),11);
 app.themeBtn=button(side,tr("Board: Nordic blue","Bräde: Nordiskt blått"),13);
 app.lessonBox=g.gtk_box_new(1,9);g.gtk_box_append(side,app.lessonBox);
 app.lessonTitle=label(app.lessonBox,"",true,"heading");
 app.lessonProgress=label(app.lessonBox,"",true,"wenuy-dim");
 app.lessonProfile=label(app.lessonBox,"",true,"wenuy-dim");
 app.lessonTask=label(app.lessonBox,"",true);
 app.lessonFeedback=label(app.lessonBox,"",true);
 Ptr lessonButtons=g.gtk_box_new(0,5);g.gtk_box_append(app.lessonBox,lessonButtons);
 button(lessonButtons,tr("Previous","Föreg."),7);button(lessonButtons,tr("Hint","Ledtråd"),8);button(lessonButtons,tr("Next","Nästa"),9);
 button(app.lessonBox,tr("Recommended practice","Rekommenderad övning"),19);
 button(app.lessonBox,tr("Exit chess school","Avsluta schackskolan"),6);
 label(side,tr("MOVE HISTORY","DRAGLISTA"),false,"heading");
 Ptr scroll=g.gtk_scrolled_window_new();g.gtk_widget_set_vexpand(scroll,1);
 g.gtk_widget_set_size_request(scroll,240,80);g.gtk_box_append(side,scroll);
 app.history=g.gtk_label_new(tr("No moves yet","Inga drag ännu"));g.gtk_label_set_xalign(app.history,0);
 g.gtk_label_set_yalign(app.history,0);margin(app.history,8,10);
 g.gtk_scrolled_window_set_child(scroll,app.history);
 button(side,tr("Export game (PGN)","Exportera parti (PGN)"),10);
 label(side,tr("Drag or click pieces • Ctrl+N new • Ctrl+Z undo • Ctrl+H hint • Ctrl+L focus","Dra eller klicka pjäser • Ctrl+N nytt • Ctrl+Z ångra • Ctrl+H tips • Ctrl+L fokus"),true,"wenuy-dim");
 label(side,"Offline • C++17 • WenuyOS",false,"dim-label");
 connect(app.window,"notify::width",onWindowWidth,nullptr);
 theme();schack_system::applyStyle();refresh();g.gtk_window_present(app.window);responsive();
 g_timeout_add(120,pollLan,nullptr);startAI();
}
} // namespace

int main(int argc,char** argv){
 // This is needed for gettext to honour LANG/LC_MESSAGES/LANGUAGE at process start.
 schack_system::initI18n();
 if(gtk_get_major_version()<4){std::fputs("GTK4 required\n",stderr);return 5;}
 if(!g.load()){
  std::fprintf(stderr,"%s\n",tr("Wenuy Chess requires GTK4. Install libgtk-4-1.","Wenuy Schack kräver GTK4. Installera paketet libgtk-4-1."));
  return 2;
 }
 std::string error;
 if(!chess::validateLessons(&error)){
  std::fprintf(stderr,"%s: %s\n",tr("Chess lessons have a rules problem","Schackskolan har ett regelproblem"),error.c_str());return 3;
 }
 load();loadProgress();
 Ptr application=g.gtk_application_new("se.wenuyos.app.schack",0);
 if(!application){std::fprintf(stderr,"%s\n",tr("Could not create GTK application.","Kunde inte skapa GTK-applikationen."));return 4;}
 connect(application,"activate",activate,nullptr);
 const int code=g_application_run(application,argc,argv);
 app.closed=true;g_object_unref(application);
 return code;
}
