// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <memory>
namespace wenuystudio {
struct BlockMeter{float left=0,right=0;};
class BlockMixer final {
public:
 explicit BlockMixer(Project project);
 ~BlockMixer();
 BlockMixer(BlockMixer&&) noexcept;
 BlockMixer& operator=(BlockMixer&&) noexcept;
 std::uint32_t sampleRate()const noexcept;
 std::uint64_t totalFrames()const noexcept;
 std::size_t process(std::uint64_t start,Stereo* out,std::size_t count,BlockMeter* m=nullptr);
private:
 struct Impl;
 std::unique_ptr<Impl> p_;
};
}
