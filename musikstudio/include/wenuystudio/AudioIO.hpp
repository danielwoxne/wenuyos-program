// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Engine.hpp"
#include "AudioBuffer.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace wenuystudio {
// Hardware-independent list of what PortAudio reports. Query never starts
// recording or playback. A missing PortAudio runtime is reported as an error.
struct AudioDevice {
 int index = -1;
 std::string name;
 std::string hostApi;
 int inputChannels = 0;
 int outputChannels = 0;
 double defaultSampleRate = 0.0;
 bool defaultInput = false;
 bool defaultOutput = false;
};
struct AudioDeviceReport {
 std::vector<AudioDevice> devices;
 int defaultInput = -1;
 int defaultOutput = -1;
};
AudioDeviceReport queryAudioDevices();
// Explicit opt-in device-open test. Opens each default stream and starts it,
// but never reads from the microphone or plays audible samples.
// Listing devices alone is NOT a hardware verification.
struct AudioProbeReport {
 bool inputOpened=false;
 bool outputOpened=false;
 std::string inputError;
 std::string outputError;
};
AudioProbeReport probeDefaultAudioDevices();

// Presets trade reduced latency against higher risk of audio dropouts.
// The PortAudio backend/host may choose a different physical latency.
enum class BufferPreset { Low=0, Balanced=1, Safe=2 };
struct BufferSettings {
 std::size_t callbackFrames=512;
 std::size_t capacityFrames=8192;
 std::size_t prefillFrames=2048;
};
BufferSettings audioBufferSettings(BufferPreset preset) noexcept;
struct PlaybackDiagnostics {
 std::uint64_t outputFrames=0; // Frames submitted to PortAudio callback, not DAC timestamp.
 std::uint64_t underruns=0; // Callback requested more samples than were available.
 std::uint32_t bufferedFrames=0;
 std::uint32_t bufferCapacityFrames=0;
 double bufferedMilliseconds=0;
 double requestedCallbackMilliseconds=0;
};

// First audio-device backend: PortAudio is loaded dynamically and remains
// optional. Disk work happens in a dedicated worker, never on the GTK thread.
// Future realtime DSP callback/ring buffer will be a separate backend.
class Playback {
public:
 Playback()=default;
 Playback(const Playback&)=delete;Playback& operator=(const Playback&)=delete;
 ~Playback();
 void start(Project project,double fromSeconds=0,BufferPreset preset=BufferPreset::Balanced);
 void stop();
 bool active() const noexcept{return active_.load();}
 double positionSeconds() const noexcept;
 float peakLeft() const noexcept{return peakLeft_.load();}
 float peakRight() const noexcept{return peakRight_.load();}
 std::string error() const;
 PlaybackDiagnostics diagnostics() const noexcept;
private:
 void run(Project project,double fromSeconds,BufferPreset preset) noexcept;
 mutable std::mutex errorMutex_;
 std::thread worker_;
 std::atomic<bool> active_{false};
 std::atomic<std::uint64_t> position_{0};
 std::atomic<std::uint32_t> rate_{48000};
 std::atomic<float> peakLeft_{0.0f},peakRight_{0.0f};
 std::atomic<std::uint64_t> underruns_{0},delivered_{0};
 std::atomic<std::uint32_t> buffered_{0},bufferCapacity_{0},callbackFrames_{512};
 std::string error_;
};
class Recording {
public:
 Recording()=default;
 Recording(const Recording&)=delete;Recording& operator=(const Recording&)=delete;
 ~Recording();
 void start(const std::string& targetWav,std::uint32_t sampleRate);
 void stop();
 bool active() const noexcept{return active_.load();}
 std::uint64_t frames() const noexcept{return frames_.load();}
 std::string error() const;
private:
 void run(std::string target,std::uint32_t sampleRate) noexcept;
 mutable std::mutex errorMutex_;
 std::thread worker_;
 std::atomic<bool> active_{false};
 std::atomic<std::uint64_t> frames_{0};
 std::string error_;
};
}
