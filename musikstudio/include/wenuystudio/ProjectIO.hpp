// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <string>
namespace wenuystudio {
// UTF-8, versioned, line-oriented .wstudio file. Paths are relative to the project file.
// Save uses a sidecar .part and keeps the previous valid save in .bak.
void saveProject(const Project& project, const std::string& path);
Project loadProject(const std::string& path);
}
