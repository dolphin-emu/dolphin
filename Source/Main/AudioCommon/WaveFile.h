// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ---------------------------------------------------------------------------------
// Class: WaveFileWriter
// Description: Simple utility class to make it easy to write long 16-bit stereo
// audio streams to disk.
// Use Start() to start recording to a file, and AddStereoSamples to add wave data.
// The float variant will convert from -1.0-1.0 range and clamp.
// Alternatively, AddSamplesBE for big endian wave data.
// If Stop is not called when it destructs, the destructor will call Stop().
// ---------------------------------------------------------------------------------

#pragma once

#include <array>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/File.h"

class WaveFileWriter
{
public:
  WaveFileWriter();
  ~WaveFileWriter();

  WaveFileWriter(const WaveFileWriter&) = delete;
  WaveFileWriter& operator=(const WaveFileWriter&) = delete;
  WaveFileWriter(WaveFileWriter&&) = delete;
  WaveFileWriter& operator=(WaveFileWriter&&) = delete;

  bool Start(const std::string& filename, unsigned int HLESampleRate);
  void Stop();

  void SetSkipSilence(bool skip) { skip_silence = skip; }
  void AddStereoSamplesBE(const short* sample_data, u32 count, int sample_rate);  // big endian
  u32 GetAudioSize() const { return audio_size; }

private:
  static constexpr size_t BUFFER_SIZE = 32 * 1024;

  File::IOFile file;
  bool skip_silence = false;
  u32 audio_size = 0;
  std::array<short, BUFFER_SIZE> conv_buffer{};
  void Write(u32 value);
  void Write4(const char* ptr);
  std::string basename;
  int current_sample_rate;
  int file_index = 0;
};
