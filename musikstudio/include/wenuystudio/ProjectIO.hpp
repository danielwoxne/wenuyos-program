// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <string>
namespace wenuystudio {
void saveProject(const Project&,const std::string& path);
Project loadProject(const std::string& path);
}
