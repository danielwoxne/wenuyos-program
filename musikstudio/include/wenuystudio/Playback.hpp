// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
namespace wenuystudio {
class Recording {
public:
 ~Recording(){stop();}
 void start(const std::string& filename,std::uint32_t sampleRate);
 void stop();
 bool active()const noexcept{return active_.load();}
 std::string error()const;
 bool completed()const noexcept{return completed_.load();}
private:
 void run(std::string target,std::uint32_t rate)noexcept;
 std::thread thread_;
 std::atomic<bool> active_{false},completed_{false};
 mutable std::mutex mutex_;
 std::string error_;
};
class Playback {
public:
 ~Playback(){stop();}
 void start(const Project&,double from=0);
 void stop();
 bool active()const noexcept {return active_.load();}
 double position()const noexcept {return double(frame_.load())/rate_.load();}
 float levelL()const noexcept{return left_.load();}
 float levelR()const noexcept{return right_.load();}
 std::string error()const;
private:
 void run(Project,double)noexcept;
 std::thread thread_;
 std::atomic<bool> active_{false};
 std::atomic<std::uint64_t> frame_{0};
 std::atomic<std::uint32_t> rate_{48000};
 std::atomic<float> left_{0},right_{0};
 mutable std::mutex mutex_;
 std::string error_;
};
}
