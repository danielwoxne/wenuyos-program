// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/MidiInput.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <string>
int main(){
 wenuystudio::MidiInput input;
 assert(!input.supported()&&!input.connected());
 std::string error;
 const auto sources=input.scan(error);
 assert(sources.empty()&&!error.empty());
 error.clear();
 assert(!input.connect({64,0,"source"},[](const auto&){},error));
 assert(!error.empty()&&!input.connected());
 input.disconnect();input.disconnect();
 std::cout<<"PASS: MIDI works without ALSA SDK or an attached device\n";
}
