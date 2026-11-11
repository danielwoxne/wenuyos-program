// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <cstddef>
#include <string>
#include <vector>
namespace wenuystudio {
enum class MediaIssueKind { Missing, Unreadable, OutOfRange };
struct MediaIssue {
 std::size_t trackIndex=0;
 std::size_t clipIndex=0;
 MediaIssueKind kind=MediaIssueKind::Missing;
 std::string filename;
 std::string details;
};
// Metadata-only inspection. No audio samples buffered in RAM.
std::vector<MediaIssue> inspectMedia(const Project& project);
const char* mediaIssueName(MediaIssueKind kind) noexcept;
}
