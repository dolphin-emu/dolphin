// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/WaveFile.h"
#include "AudioCommon/Mixer.h"

#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

constexpr size_t WaveFileWriter::BUFFER_SIZE;

WaveFileWriter::WaveFileWriter()
{
}

WaveFileWriter::~WaveFileWriter()
{
  Stop();
}

bool WaveFileWriter::Start(const std::string& filename, u32 sample_rate_divisor)
{
  // Ask to delete file
  if (File::Exists(filename))
  {
    if (Config::Get(Config::MAIN_DUMP_AUDIO_SILENT) ||
        AskYesNoFmtT("Delete the existing file '{0}'?", filename))
    {
      File::Delete(filename);
    }
    else
    {
      // Stop and cancel dumping the audio
      return false;
    }
  }

  // Check if the file is already open
  if (file)
  {
    PanicAlertFmtT("The file {0} was already open, the file header will not be written.", filename);
    return false;
  }

  file.Open(filename, "wb");
  if (!file)
  {
    PanicAlertFmtT(
        "The file {0} could not be opened for writing. Please check if it's already opened "
        "by another program.",
        filename);
    return false;
  }

  audio_size = 0;

  if (basename.empty())
    SplitPath(filename, nullptr, &basename, nullptr);

  current_sample_rate_divisor = sample_rate_divisor;

  // -----------------
  // Write file header
  // -----------------
  Write4("RIFF");
  Write(100 * 1000 * 1000);  // write big value in case the file gets truncated
  Write4("WAVE");
  Write4("fmt ");

  Write(16);          // size of fmt block
  Write(0x00020001);  // two channels, uncompressed

  const u32 sample_rate = Mixer::FIXED_SAMPLE_RATE_DIVIDEND / sample_rate_divisor;
  Write(sample_rate);
  Write(sample_rate * 2 * 2);  // two channels, 16bit

  Write(0x00100004);
  Write4("data");
  Write(100 * 1000 * 1000 - 32);

  // We are now at offset 44
  if (file.Tell() != 44)
    PanicAlertFmt("Wrong offset: {}", file.Tell());

  return true;
}

void WaveFileWriter::Stop()
{
  file.Seek(4, File::SeekOrigin::Begin);
  Write(audio_size + 36);

  file.Seek(40, File::SeekOrigin::Begin);
  Write(audio_size);

  file.Close();
}

void WaveFileWriter::Write(u32 value)
{
  file.WriteArray(&value, 1);
}

void WaveFileWriter::Write4(const char* ptr)
{
  file.WriteBytes(ptr, 4);
}

void WaveFileWriter::AddStereoSamplesBE(const short* sample_data, u32 count,
                                        u32 sample_rate_divisor, int l_volume, int r_volume)
{
  if (!file)
  {
    ERROR_LOG_FMT(AUDIO, "WaveFileWriter - file not open.");
    return;
  }

  if (count * 2 > BUFFER_SIZE)
  {
    ERROR_LOG_FMT(AUDIO, "WaveFileWriter - buffer too small (count = {}).", count);
    return;
  }

  if (skip_silence)
  {
    bool all_zero = true;

    for (u32 i = 0; i < count * 2; i++)
    {
      if (sample_data[i])
        all_zero = false;
    }

    if (all_zero)
      return;
  }

  for (u32 i = 0; i < count; i++)
  {
    // Flip the audio channels from RL to LR
    conv_buffer[2 * i] = Common::swap16((u16)sample_data[2 * i + 1]);
    conv_buffer[2 * i + 1] = Common::swap16((u16)sample_data[2 * i]);

    // Apply volume (volume ranges from 0 to 256)
    conv_buffer[2 * i] = conv_buffer[2 * i] * l_volume / 256;
    conv_buffer[2 * i + 1] = conv_buffer[2 * i + 1] * r_volume / 256;
  }

  if (sample_rate_divisor != current_sample_rate_divisor)
  {
    Stop();
    file_index++;
    const std::string filename =
        fmt::format("{}{}{}.wav", File::GetUserPath(D_DUMPAUDIO_IDX), basename, file_index);
    Start(filename, sample_rate_divisor);
    current_sample_rate_divisor = sample_rate_divisor;
  }

  file.WriteBytes(conv_buffer.data(), count * 4);
  audio_size += count * 4;
}
