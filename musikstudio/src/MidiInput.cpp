// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/MidiInput.hpp"
#include <utility>

#ifdef WENUYSTUDIO_HAS_ALSA
#include <alsa/asoundlib.h>
#include <cerrno>
#include <glib-unix.h>
#include <poll.h>
#include <string>
#include <vector>
#endif

namespace wenuystudio {

struct MidiInput::Impl {
#ifdef WENUYSTUDIO_HAS_ALSA
 snd_seq_t* seq=nullptr;
 int localPort=-1;
 MidiInputSource source{};
 Sink sink{};
 std::vector<guint> watches{};

 bool active()const noexcept {return seq!=nullptr;}
 void close()noexcept {
  for(auto id:watches)if(id)g_source_remove(id);
  watches.clear();
  if(seq) {
   if(localPort>=0&&source.client>=0)
    snd_seq_disconnect_from(seq,localPort,source.client,source.port);
   snd_seq_close(seq);seq=nullptr;
  }
  localPort=-1;source={};sink={};
 }
 void emit(const MidiInputMessage& message) noexcept {
  try{if(sink)sink(message);}
  catch(...){g_warning("MIDI event handler failed");}
 }
 void lost()noexcept {
  // The callback runs on the GTK main loop. Release any notes on connection loss.
  auto callback=sink;
  close();
  try{if(callback)callback({MidiInputKind::Disconnected,-1,0.f,-1});}
  catch(...){g_warning("MIDI disconnect handler failed");}
 }
 static gboolean ready(gint,GIOCondition flags,gpointer data) {
  auto* self=static_cast<Impl*>(data);
  if(!self->active())return G_SOURCE_REMOVE;
  if(flags&(G_IO_HUP|G_IO_ERR|G_IO_NVAL)){self->lost();return G_SOURCE_REMOVE;}
  // Per-dispatch limit prevents a noisy device from blocking GTK input.
  for(int count=0;count<128;++count) {
   snd_seq_event_t* event=nullptr;
   const int rc=snd_seq_event_input(self->seq,&event);
   if(rc==-EAGAIN)break;
   if(rc<0){self->lost();return G_SOURCE_REMOVE;}
   if(!event)break;
   const bool fromSource=event->source.client==self->source.client&&
                         event->source.port==self->source.port;
   if(fromSource) {
    std::optional<MidiInputMessage> message;
    switch(event->type) {
     case SND_SEQ_EVENT_NOTEON:
      message=midiNoteMessage(true,event->data.note.note,event->data.note.velocity,
                              event->data.note.channel);break;
     case SND_SEQ_EVENT_NOTEOFF:
      message=midiNoteMessage(false,event->data.note.note,event->data.note.velocity,
                              event->data.note.channel);break;
     case SND_SEQ_EVENT_CONTROLLER:
      message=midiControlMessage(event->data.control.param,event->data.control.channel);break;
     default:break;
    }
    if(message)self->emit(*message);
   }
   // Current ALSA releases own the input-event storage. Do not free event.
   if(!self->active())return G_SOURCE_REMOVE;
   // ALSA returns *remaining bytes*, which can be zero on a successful
   // event fetch. Always process the event before checking the count.
   if(rc==0)break;
  }
  return G_SOURCE_CONTINUE;
 }
#else
 bool active()const noexcept{return false;}
 void close()noexcept{}
#endif
 ~Impl(){close();}
};

MidiInput::MidiInput():impl_(std::make_unique<Impl>()){}
MidiInput::~MidiInput()=default;
bool MidiInput::supported()const noexcept {
#ifdef WENUYSTUDIO_HAS_ALSA
 return true;
#else
 return false;
#endif
}
bool MidiInput::connected()const noexcept{return impl_->active();}
void MidiInput::disconnect()noexcept{impl_->close();}

std::vector<MidiInputSource> MidiInput::scan(std::string& error)const {
 error.clear();
 std::vector<MidiInputSource> found;
#ifdef WENUYSTUDIO_HAS_ALSA
 snd_seq_t* seq=nullptr;
 if(snd_seq_open(&seq,"default",SND_SEQ_OPEN_INPUT,SND_SEQ_NONBLOCK)<0){
  error="The ALSA sequencer could not be opened";return found;
 }
 snd_seq_client_info_t* clientInfo=nullptr;
 snd_seq_port_info_t* portInfo=nullptr;
 if(snd_seq_client_info_malloc(&clientInfo)<0||
    snd_seq_port_info_malloc(&portInfo)<0){
  error="Not enough memory for MIDI device discovery";
 }else{
  snd_seq_client_info_set_client(clientInfo,-1);
  while(found.size()<128&&snd_seq_query_next_client(seq,clientInfo)>=0) {
   const int client=snd_seq_client_info_get_client(clientInfo);
   if(client<=0||client==snd_seq_client_id(seq)||client>=255)continue;
   snd_seq_port_info_set_client(portInfo,client);
   snd_seq_port_info_set_port(portInfo,-1);
   while(found.size()<128&&snd_seq_query_next_port(seq,portInfo)>=0) {
    const int port=snd_seq_port_info_get_port(portInfo);
    const unsigned int caps=snd_seq_port_info_get_capability(portInfo);
    if(port<0||port>=255||
       (caps&(SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ))!=
             (SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ)||
       (caps&SND_SEQ_PORT_CAP_INACTIVE))continue;
    const char* c=snd_seq_client_info_get_name(clientInfo);
    const char* p=snd_seq_port_info_get_name(portInfo);
    std::string label=std::string(c?c:"MIDI")+" / "+(p?p:"Port");
    if(label.size()>160)label.resize(160);
    found.push_back({client,port,label+" ("+std::to_string(client)+":"+
                            std::to_string(port)+")"});
   }
  }
 }
 if(portInfo)snd_seq_port_info_free(portInfo);
 if(clientInfo)snd_seq_client_info_free(clientInfo);
 snd_seq_close(seq);
#else
 error="ALSA support was not built";
#endif
 return found;
}

bool MidiInput::connect(const MidiInputSource& source,Sink sink,std::string& error){
 error.clear();
 disconnect();
 if(!sink||source.client<=0||source.client>=255||source.port<0||
    source.port>=255) {
  error="Invalid MIDI input source";return false;
 }
#ifdef WENUYSTUDIO_HAS_ALSA
 snd_seq_t* seq=nullptr;
 if(snd_seq_open(&seq,"default",SND_SEQ_OPEN_INPUT,SND_SEQ_NONBLOCK)<0){
  error="ALSA sequencer is not available";return false;
 }
 // The receiver is not exported to unrelated apps. Only the user-selected
 // source is subscribed; there is no auto-connect or MIDI-over-network.
 snd_seq_set_client_name(seq,"Wenuy Studio");
 const int port=snd_seq_create_simple_port(seq,"Music keyboard input",
                        SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT,
                        SND_SEQ_PORT_TYPE_MIDI_GENERIC|SND_SEQ_PORT_TYPE_APPLICATION);
 if(port<0){
  error="Could not create MIDI input port";snd_seq_close(seq);return false;
 }
 if(snd_seq_connect_from(seq,port,source.client,source.port)<0){
  error="Could not subscribe to the selected MIDI device";snd_seq_close(seq);return false;
 }
 const int count=snd_seq_poll_descriptors_count(seq,POLLIN);
 if(count<1||count>8){
  error="MIDI device returned unsupported file descriptors";
  snd_seq_close(seq);return false;
 }
 std::vector<struct pollfd> descriptors(static_cast<std::size_t>(count));
 if(snd_seq_poll_descriptors(seq,descriptors.data(),static_cast<unsigned>(count),POLLIN)!=count){
  error="Could not watch the selected MIDI device";snd_seq_close(seq);return false;
 }
 impl_->seq=seq;impl_->localPort=port;impl_->source=source;impl_->sink=std::move(sink);
 for(const auto& desc:descriptors){
  const guint id=g_unix_fd_add_full(G_PRIORITY_DEFAULT,desc.fd,
                  static_cast<GIOCondition>(G_IO_IN|G_IO_HUP|G_IO_ERR|G_IO_NVAL),
                  &Impl::ready,impl_.get(),nullptr);
  if(!id){
   error="Could not add the MIDI event watcher";disconnect();return false;
  }
  impl_->watches.push_back(id);
 }
 return true;
#else
 error="ALSA support was not built";
 return false;
#endif
}
} // namespace wenuystudio
