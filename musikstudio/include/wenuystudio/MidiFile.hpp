// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <string>
namespace wenuystudio {
// Standard MIDI File (SMF 0/1), note on/off and tempo map.
// Instruments, controllers, sysex and automation are not imported.
Project importMidi(const std::string& filename);
// Export note tracks; tempo is fixed at project.bpm.
void exportMidi(const Project& project, const std::string& filename);
}
