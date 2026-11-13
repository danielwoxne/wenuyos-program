// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>

namespace wenuystudio {
struct ImportedAudio {
 std::string filename; // stable path to a WAV playable by the block mixer
 std::uint32_t rate=0;
 std::uint64_t frames=0;
 bool converted=false;
};
// WAV works without WenuyOS Datatyper. Other formats use the optional system
// decoder and are cached as owned WAVs in permanent per-user application data.
// The caller supplies a destination directory, so this function is unit-testable.
ImportedAudio importAudioFile(const std::string& path,const std::string& ownedMediaDirectory);
bool supportsSystemAudioDatatypes() noexcept;
}
