// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Microphone.h"

#include "Common/Swap.h"

#include <algorithm>

namespace IOS::HLE::USB
{
std::vector<std::string> Microphone::ListDevices()
{
  std::vector<std::string> devices{};
  const ALchar* pDeviceList = alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
  while (*pDeviceList)
  {
    devices.emplace_back(pDeviceList);
    pDeviceList += strlen(pDeviceList) + 1;
  }

  return devices;
}

int Microphone::OpenMicrophone()
{
  m_device = alcCaptureOpenDevice(nullptr, SAMPLING_RATE, AL_FORMAT_MONO16, BUFFER_SIZE);
  m_dsp_data.resize(BUFFER_SIZE, 0);
  m_temp_buffer.resize(BUFFER_SIZE, 0);
  return static_cast<int>(alcGetError(m_device));
}

int Microphone::StartCapture()
{
  alcCaptureStart(m_device);
  return static_cast<int>(alcGetError(m_device));
}

void Microphone::StopCapture()
{
  alcCaptureStop(m_device);
}

void Microphone::PerformAudioCapture()
{
  m_num_of_samples = BUFFER_SIZE / 2;

  ALCint samples_in{};
  alcGetIntegerv(m_device, ALC_CAPTURE_SAMPLES, 1, &samples_in);
  m_num_of_samples = std::min(m_num_of_samples, static_cast<u32>(samples_in));

  if (m_num_of_samples == 0)
    return;

  alcCaptureSamples(m_device, m_dsp_data.data(), m_num_of_samples);
}

void Microphone::ByteSwap(const void* src, void* dst) const
{
  *static_cast<u16*>(dst) = Common::swap16(*static_cast<const u16*>(src));
}

void Microphone::GetSoundData()
{
  if (m_num_of_samples == 0)
    return;

  u8* ptr = const_cast<u8*>(m_temp_buffer.data());
  // Convert LE to BE
  for (u32 i = 0; i < m_num_of_samples; i++)
  {
    for (u32 indchan = 0; indchan < 1; indchan++)
    {
      const u32 curindex = (i * 2) + indchan * (16 / 8);
      ByteSwap(m_dsp_data.data() + curindex, ptr + curindex);
    }
  }

  m_rbuf_dsp.write_bytes(ptr, m_num_of_samples * 2);
}

void Microphone::ReadIntoBuffer(u8* dst, u32 size)
{
  m_rbuf_dsp.read_bytes(dst, size);
}

bool Microphone::HasData() const
{
  return m_num_of_samples != 0;
}
}  // namespace IOS::HLE::USB
