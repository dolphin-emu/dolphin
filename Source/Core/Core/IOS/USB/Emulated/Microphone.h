// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <OpenAL/al.h>
#include <OpenAL/alc.h>

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace IOS::HLE::USB
{
template <size_t S>
class simple_ringbuf
{
public:
  simple_ringbuf() { m_container.resize(S); }

  bool has_data() const { return m_used != 0; }

  u32 read_bytes(u8* buf, const u32 size)
  {
    u32 to_read = size > m_used ? m_used : size;
    if (!to_read)
      return 0;

    u8* data = m_container.data();
    u32 new_tail = m_tail + to_read;

    if (new_tail >= S)
    {
      u32 first_chunk_size = S - m_tail;
      std::memcpy(buf, data + m_tail, first_chunk_size);
      std::memcpy(buf + first_chunk_size, data, to_read - first_chunk_size);
      m_tail = (new_tail - S);
    }
    else
    {
      std::memcpy(buf, data + m_tail, to_read);
      m_tail = new_tail;
    }

    m_used -= to_read;

    return to_read;
  }

  void write_bytes(const u8* buf, const u32 size)
  {
    if (u32 over_size = m_used + size; over_size > S)
    {
      m_tail += (over_size - S);
      if (m_tail > S)
        m_tail -= S;

      m_used = S;
    }
    else
    {
      m_used = over_size;
    }

    u8* data = m_container.data();
    u32 new_head = m_head + size;

    if (new_head >= S)
    {
      u32 first_chunk_size = S - m_head;
      std::memcpy(data + m_head, buf, first_chunk_size);
      std::memcpy(data, buf + first_chunk_size, size - first_chunk_size);
      m_head = (new_head - S);
    }
    else
    {
      std::memcpy(data + m_head, buf, size);
      m_head = new_head;
    }
  }

protected:
  std::vector<u8> m_container;
  u32 m_head = 0, m_tail = 0, m_used = 0;
};

class Microphone final
{
public:
  static std::vector<std::string> ListDevices();

  int OpenMicrophone();
  int StartCapture();
  void StopCapture();
  void PerformAudioCapture();
  void GetSoundData();
  void ReadIntoBuffer(u8* dst, u32 size);
  bool HasData() const;

private:
  void ByteSwap(const void* src, void* dst) const;

  static constexpr u32 SAMPLING_RATE = 8000;
  static constexpr u32 BUFFER_SIZE = SAMPLING_RATE / 2;

  ALCdevice* m_device;
  u32 m_num_of_samples{};
  std::vector<u8> m_dsp_data{};
  std::vector<u8> m_temp_buffer{};
  simple_ringbuf<BUFFER_SIZE> m_rbuf_dsp;
};
}  // namespace IOS::HLE::USB
