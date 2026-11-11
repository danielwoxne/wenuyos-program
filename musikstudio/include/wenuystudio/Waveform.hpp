// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace wenuystudio {
struct Peak { float min=0.0f, max=0.0f; };
// Samples a capped number of frames across the complete file. Preview only,
// does not affect export or audio fidelity, and never loads the file into RAM.
std::vector<Peak> waveform(const std::string& path,std::size_t columns=1024);
// Exact min/max per time bucket. Scans the complete file in small sequential
// chunks, so run on a background worker, never the GTK UI or audio threads.
// The returned vector uses at most 4096 * sizeof(Peak) bytes.
std::vector<Peak> waveformAccurate(const std::string& path,std::size_t columns=1024);
}
