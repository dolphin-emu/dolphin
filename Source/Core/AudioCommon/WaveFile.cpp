// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/WaveFile.h"

#include <string>

#include <fmt/format.h>

#include "AudioCommon/Mixer.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"

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
  if (m_file)
  {
    PanicAlertFmtT("The file {0} was already open, the file header will not be written.", filename);
    return false;
  }

  m_file.Open(filename, "wb");
  if (!m_file)
  {
    PanicAlertFmtT(
        "The file {0} could not be opened for writing. Please check if it's already opened "
        "by another program.",
        filename);
    return false;
  }

  m_audio_size = 0;

  if (m_basename.empty())
    SplitPath(filename, nullptr, &m_basename, nullptr);

  m_current_sample_rate_divisor = sample_rate_divisor;

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
  if (m_file.Tell() != 44)
    PanicAlertFmt("Wrong offset: {}", m_file.Tell());

  return true;
}

void WaveFileWriter::Stop()
{
  m_file.Seek(4, File::SeekOrigin::Begin);
  Write(m_audio_size + 36);

  m_file.Seek(40, File::SeekOrigin::Begin);
  Write(m_audio_size);

  m_file.Close();
}

void WaveFileWriter::Write(u32 value)
{
  m_file.WriteArray(&value, 1);
}

void WaveFileWriter::Write4(const char* ptr)
{
  m_file.WriteBytes(ptr, 4);
}

void WaveFileWriter::AddStereoSamplesBE(const short* sample_data, u32 count,
                                        u32 sample_rate_divisor, int l_volume, int r_volume)
{
  if (!m_file)
  {
    ERROR_LOG_FMT(AUDIO, "WaveFileWriter - file not open.");
    return;
  }

  if (count * 2 > BUFFER_SIZE)
  {
    ERROR_LOG_FMT(AUDIO, "WaveFileWriter - buffer too small (count = {}).", count);
    return;
  }

  if (m_skip_silence)
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
    m_conv_buffer[2 * i] = Common::swap16((u16)sample_data[2 * i + 1]);
    m_conv_buffer[2 * i + 1] = Common::swap16((u16)sample_data[2 * i]);

    // Apply volume (volume ranges from 0 to 256)
    m_conv_buffer[2 * i] = m_conv_buffer[2 * i] * l_volume / 256;
    m_conv_buffer[2 * i + 1] = m_conv_buffer[2 * i + 1] * r_volume / 256;
  }

  if (sample_rate_divisor != m_current_sample_rate_divisor)
  {
    Stop();
    m_file_index++;
    const std::string filename =
        fmt::format("{}{}{}.wav", File::GetUserPath(D_DUMPAUDIO_IDX), m_basename, m_file_index);
    Start(filename, sample_rate_divisor);
    m_current_sample_rate_divisor = sample_rate_divisor;
  }

  m_file.WriteBytes(m_conv_buffer.data(), count * 4);
  m_audio_size += count * 4;
}

bool AudioCommon::LoadWavFile(const std::string& filename, std::vector<s16>* out_data,
                              u32* out_sample_rate, u16* out_channels)
{
  File::IOFile file(filename, "rb");
  if (!file)
    return false;

  char magic[4];
  if (!file.ReadBytes(magic, 4) || strncmp(magic, "RIFF", 4) != 0)
    return false;
  file.Seek(4, File::SeekOrigin::Current);
  if (!file.ReadBytes(magic, 4) || strncmp(magic, "WAVE", 4) != 0)
    return false;

  bool found_fmt = false;
  bool found_data = false;

  while (file.ReadBytes(magic, 4))
  {
    u32 chunk_size;
    if (!file.ReadArray(&chunk_size, 1))
      break;

    if (strncmp(magic, "fmt ", 4) == 0)
    {
      u16 audio_format;
      file.ReadArray(&audio_format, 1);
      file.ReadArray(out_channels, 1);
      file.ReadArray(out_sample_rate, 1);
      file.Seek(6, File::SeekOrigin::Current);
      u16 bits_per_sample;
      file.ReadArray(&bits_per_sample, 1);

      if (audio_format != 1 || bits_per_sample != 16)
        return false;

      if (chunk_size > 16)
        file.Seek(chunk_size - 16, File::SeekOrigin::Current);
      found_fmt = true;
    }
    else if (strncmp(magic, "data", 4) == 0)
    {
      out_data->resize(chunk_size / 2);
      file.ReadArray(out_data->data(), out_data->size());
      found_data = true;
      if (chunk_size % 2 != 0)
        file.Seek(1, File::SeekOrigin::Current);
    }
    else
    {
      file.Seek((chunk_size + 1) & ~1, File::SeekOrigin::Current);
    }
  }

  return found_fmt && found_data;
}
