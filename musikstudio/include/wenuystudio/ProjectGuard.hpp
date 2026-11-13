// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <optional>
#include <string>
#include <utility>

namespace wenuystudio {
// The same decision gate is used for New, Open (including file associations)
// and Close. No GTK dependency: the transitions are tested headlessly.
enum class ProjectActionKind {New,Open,Close};
struct ProjectAction {
 ProjectActionKind kind;
 std::string path;
};
enum class GuardDecision {Proceed,Prompt,Busy};
class ProjectGuard final {
public:
 GuardDecision request(ProjectAction action,bool dirty,bool recordingOrDialogOpen) {
  if(recordingOrDialogOpen||pending_)return GuardDecision::Busy;
  if(!dirty)return GuardDecision::Proceed;
  pending_=std::move(action);
  return GuardDecision::Prompt;
 }
 bool hasPending() const noexcept {return pending_.has_value();}
 bool saving() const noexcept {return saving_;}
 void cancel() noexcept {pending_.reset();saving_=false;}
 std::optional<ProjectAction> discard() {
  if(saving_)return std::nullopt;
  auto result=std::move(pending_);
  cancel();
  return result;
 }
 bool beginSave() noexcept {
  if(!pending_||saving_)return false;
  saving_=true;
  return true;
 }
 // Failure or cancellation never releases the destructive action.
 std::optional<ProjectAction> finishedSave(bool successful) {
  if(!saving_)return std::nullopt;
  saving_=false;
  auto result=std::move(pending_);
  pending_.reset();
  return successful?std::move(result):std::nullopt;
 }
private:
 std::optional<ProjectAction> pending_;
 bool saving_=false;
};
} // namespace wenuystudio
