// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <string>
namespace wenuystudio {
// Sidecars for named projects; unsaved projects use XDG_STATE_HOME.
// Recovery does not replace a saved .wstudio without explicit user consent.
std::string recoveryPath(const std::string& projectPath);
void saveRecovery(const Project& project,const std::string& projectPath);
bool hasRecovery(const std::string& projectPath) noexcept;
Project loadRecovery(const std::string& projectPath);
void discardRecovery(const std::string& projectPath);
}
