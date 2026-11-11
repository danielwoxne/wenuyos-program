// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <cstddef>
#include <deque>
namespace wenuystudio {
// Undo stack stores only models, never sample buffers; total budget is bounded.
class History {
public:
 explicit History(std::size_t maxBytes=16*1024*1024, std::size_t maxSteps=32);
 void checkpoint(const Project& project);
 bool undo(Project& project);
 bool redo(Project& project);
 void clear() noexcept;
 bool canUndo() const noexcept { return !undo_.empty(); }
 bool canRedo() const noexcept { return !redo_.empty(); }
 std::size_t memoryBytes() const noexcept;
private:
 static std::size_t weight(const Project& project) noexcept;
 void trim();
 std::size_t budget_,steps_;
 std::deque<Project> undo_, redo_;
};
}
