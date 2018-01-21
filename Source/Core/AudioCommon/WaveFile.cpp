// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/WaveFile.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/ConfigManager.h"

#if defined(_MSC_VER) && _MSC_VER <= 1800
#undef BUFFER_SIZE
#define BUFFER_SIZE WAVE_WRITER_BUFFER_SIZE
#else
constexpr size_t WaveFileWriter::BUFFER_SIZE;
#endif

WaveFileWriter::WaveFileWriter()
{
}

WaveFileWriter::~WaveFileWriter()
{
  Stop();
}

bool WaveFileWriter::Start(const std::string& filename, unsigned int HLESampleRate)
{
  // Ask to delete file
  if (File::Exists(filename))
  {
    if (SConfig::GetInstance().m_DumpAudioSilent ||
        AskYesNoT("Delete the existing file '%s'?", filename.c_str()))
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
    PanicAlertT("The file %s was already open, the file header will not be written.",
                filename.c_str());
    return false;
  }

  file.Open(filename, "wb");
  if (!file)
  {
    PanicAlertT("The file %s could not be opened for writing. Please check if it's already opened "
                "by another program.",
                filename.c_str());
    return false;
  }

  audio_size = 0;

  if (basename.empty())
    SplitPath(filename, nullptr, &basename, nullptr);

  current_sample_rate = HLESampleRate;

  // -----------------
  // Write file header
  // -----------------
  Write4("RIFF");
  Write(100 * 1000 * 1000);  // write big value in case the file gets truncated
  Write4("WAVE");
  Write4("fmt ");

  Write(16);          // size of fmt block
  Write(0x00020001);  // two channels, uncompressed

  const u32 sample_rate = HLESampleRate;
  Write(sample_rate);
  Write(sample_rate * 2 * 2);  // two channels, 16bit

  Write(0x00100004);
  Write4("data");
  Write(100 * 1000 * 1000 - 32);

  // We are now at offset 44
  if (file.Tell() != 44)
    PanicAlert("Wrong offset: %lld", (long long)file.Tell());

  return true;
}

void WaveFileWriter::Stop()
{
  // u32 file_size = (u32)ftello(file);
  file.Seek(4, SEEK_SET);
  Write(audio_size + 36);

  file.Seek(40, SEEK_SET);
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

void WaveFileWriter::AddStereoSamplesBE(const short* sample_data, u32 count, int sample_rate)
{
  if (!file)
    PanicAlertT("WaveFileWriter - file not open.");

  if (count > BUFFER_SIZE * 2)
    PanicAlert("WaveFileWriter - buffer too small (count = %u).", count);

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
  }

  if (sample_rate != current_sample_rate)
  {
    Stop();
    file_index++;
    std::stringstream filename;
    filename << File::GetUserPath(D_DUMPAUDIO_IDX) << basename << file_index << ".wav";
    Start(filename.str(), sample_rate);
    current_sample_rate = sample_rate;
  }

  file.WriteBytes(conv_buffer.data(), count * 4);
  audio_size += count * 4;
}
