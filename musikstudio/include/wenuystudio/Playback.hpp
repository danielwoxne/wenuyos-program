// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include <atomic>
#include <array>
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
// Persistent PortAudio audition: single GTK producer, single audio worker.
// The bounded queue drops all held voices safely if it ever overflows.
class LivePreview {
public:
 ~LivePreview(){stop();}
 void start(std::uint32_t rate);
 void stop()noexcept;
 bool active()const noexcept{return running_.load();}
 bool noteOn(int key,int instrument,float velocity)noexcept;
 bool noteOff(int key)noexcept;
 bool allNotesOff()noexcept;
 std::string error()const;
private:
 enum class Kind{On,Off,AllOff};
 struct Event{Kind kind;int key;int instrument;float velocity;};
 static constexpr unsigned capacity=256;
 std::array<Event,capacity> events_{};
 std::atomic<unsigned> written_{0},read_{0};
 std::atomic<bool> running_{false},panic_{false};
 std::thread worker_;
 std::uint32_t rate_=48000;
 mutable std::mutex mutex_;
 std::string error_;
 bool send(Event e)noexcept;
 void run()noexcept;
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
