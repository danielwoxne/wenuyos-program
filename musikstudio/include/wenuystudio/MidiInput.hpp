// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wenuystudio {
enum class MidiInputKind { NoteOn, NoteOff, AllNotesOff, Disconnected };
struct MidiInputMessage {
 MidiInputKind kind=MidiInputKind::NoteOff;
 int key=-1;
 float velocity=0.f;
 int channel=-1;
};
struct MidiInputSource {
 int client=-1,port=-1;
 std::string label;
};

// These helpers are independent of ALSA headers for tests and machines without ALSA.
inline std::optional<MidiInputMessage> midiNoteMessage(bool on,int key,int velocity,int channel) noexcept {
 if(key<0||key>127||velocity<0||velocity>127||channel<0||channel>15)return std::nullopt;
 if(on&&velocity>0)return MidiInputMessage{MidiInputKind::NoteOn,key,velocity/127.f,channel};
 return MidiInputMessage{MidiInputKind::NoteOff,key,0.f,channel};
}
inline std::optional<MidiInputMessage> midiControlMessage(int controller,int channel) noexcept {
 if(channel<0||channel>15)return std::nullopt;
 if(controller!=120&&controller!=123)return std::nullopt;
 return MidiInputMessage{MidiInputKind::AllNotesOff,-1,0.f,channel};
}

// ALSA Sequencer is an optional, event-driven local MIDI source.
// Nothing is opened or subscribed until scan() or connect() is called by the user.
class MidiInput {
public:
 using Sink=std::function<void(const MidiInputMessage&)>;
 MidiInput();
 ~MidiInput();
 MidiInput(const MidiInput&)=delete;
 MidiInput& operator=(const MidiInput&)=delete;
 bool supported()const noexcept;
 std::vector<MidiInputSource> scan(std::string& error) const;
 bool connect(const MidiInputSource& source,Sink sink,std::string& error);
 void disconnect() noexcept;
 bool connected()const noexcept;
private:
 struct Impl;
 std::unique_ptr<Impl> impl_;
};
} // namespace wenuystudio
