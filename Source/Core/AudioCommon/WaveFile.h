// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
#include "Common/IOFile.h"

class WaveFileWriter
{
public:
  WaveFileWriter();
  ~WaveFileWriter();

  WaveFileWriter(const WaveFileWriter&) = delete;
  WaveFileWriter& operator=(const WaveFileWriter&) = delete;
  WaveFileWriter(WaveFileWriter&&) = delete;
  WaveFileWriter& operator=(WaveFileWriter&&) = delete;

  bool Start(const std::string& filename, u32 sample_rate_divisor);
  void Stop();

  void SetSkipSilence(bool skip) { skip_silence = skip; }
  // big endian
  void AddStereoSamplesBE(const short* sample_data, u32 count, u32 sample_rate_divisor,
                          int l_volume, int r_volume);
  u32 GetAudioSize() const { return audio_size; }

private:
  static constexpr size_t BUFFER_SIZE = 32 * 1024;

  void Write(u32 value);
  void Write4(const char* ptr);

  File::IOFile file;
  std::string basename;
  u32 file_index = 0;
  u32 audio_size = 0;

  u32 current_sample_rate_divisor;
  std::array<short, BUFFER_SIZE> conv_buffer{};

  bool skip_silence = false;
};
