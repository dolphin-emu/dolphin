// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "AudioCommon/Microphone.h"
#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{
class GameCubeMicState final : public AudioCommon::MicrophoneState
{
public:
  static constexpr u32 DEFAULT_SAMPLING_RATE = 11025;

  bool IsSampleOn() const override;
  bool IsMuted() const override;
  u32 GetDefaultSamplingRate() const override;

  struct Status
  {
    u16 out : 4;          // MICSet/GetOut...???
    u16 id : 1;           // Used for MICGetDeviceID (always 0)
    u16 button_unk : 3;   // Button bits which appear unused
    u16 button : 1;       // The actual button on the mic
    u16 buff_ovrflw : 1;  // Ring buffer wrote over bytes which weren't read by console
    u16 gain : 1;         // Gain: 0dB or 15dB
    u16 sample_rate : 2;  // Sample rate, 00-11025, 01-22050, 10-44100, 11-??
    u16 buff_size : 2;    // Ring buffer size in bytes, 00-32, 01-64, 10-128, 11-???
    u16 is_active : 1;    // If we are sampling or not
  };

  Status status{};
};

class CEXIMic : public IEXIDevice
{
public:
  CEXIMic(Core::System& system, const int index);
  void SetCS(int cs) override;
  bool IsInterruptSet() override;
  bool IsPresent() const override;

private:
  static constexpr u8 EXI_ID[] = {0, 0x0a, 0, 0, 0};
  static constexpr int SAMPLE_SIZE = sizeof(s16);
  static constexpr int RATE_BASE = 11025;
  static constexpr int RING_BASE = 32;

  enum
  {
    cmdID = 0x00,
    cmdGetStatus = 0x40,
    cmdSetStatus = 0x80,
    cmdGetBuffer = 0x20,
    cmdReset = 0xFF,
  };

  int m_slot;

  u32 m_position = 0;
  int m_command = 0;

  void TransferByte(u8& byte) override;

  // 0 to disable interrupts, else it will be checked against current CPU ticks
  // to determine if interrupt should be raised
  u64 m_next_int_ticks = 0;
  void UpdateNextInterruptTicks();

  // status bits converted to nice numbers
  u32 m_sample_rate = RATE_BASE;
  u32 m_buff_size = RING_BASE;
  u32 m_buff_size_samples = m_buff_size / SAMPLE_SIZE;

  // Arbitrarily small ringbuffer used by audio input backend in order to
  // keep delay tolerable
  std::unique_ptr<AudioCommon::Microphone> m_microphone{};
  GameCubeMicState m_sampler{};

  // 64 is the max size, can be 16 or 32 as well
  std::array<u8, 64 * SAMPLE_SIZE> m_ring_buffer{};
  std::size_t m_ring_pos = 0;
  std::size_t m_ring_size = m_ring_buffer.size();
};
}  // namespace ExpansionInterface
