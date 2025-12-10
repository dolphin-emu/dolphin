// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceMic.h"

#include "Common/Logging/Log.h"

#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

namespace ExpansionInterface
{
bool GameCubeMicState::IsSampleOn() const
{
  return status.is_active;
}

bool GameCubeMicState::IsMuted() const
{
  return false;  // Not applicable
}

u32 GameCubeMicState::GetDefaultSamplingRate() const
{
  return DEFAULT_SAMPLING_RATE;
}

namespace
{
class MicrophoneGameCube : public AudioCommon::Microphone
{
public:
  explicit MicrophoneGameCube(const GameCubeMicState& sampler)
      : Microphone(sampler, "GameCube Microphone"), m_sampler(sampler)
  {
  }

  bool GetOverflowBit()
  {
    const bool result = m_overflow_bit_set;
    m_overflow_bit_set = false;
    return result;
  }

private:
#ifdef HAVE_CUBEB
  std::string GetInputDeviceId() const override { return {}; }
  std::string GetCubebStreamName() const override { return "Dolphin Emulated GameCube Microphone"; }
  s16 GetVolumeModifier() const override { return 0; }
  bool AreSamplesByteSwapped() const override { return true; }
#endif

  void OnOverflow() override
  {
    m_samples_avail = 0;
    m_overflow_bit_set = true;
  }

  bool IsMicrophoneMuted() const override { return false; }
  u32 GetStreamSize() const override { return BUFF_SIZE_SAMPLES * 500; }

  const GameCubeMicState& m_sampler;
  bool m_overflow_bit_set = false;
};
}  // namespace

// EXI Mic Device
// This works by opening and starting an input stream when the is_active
// bit is set. The interrupt is scheduled in the future based on sample rate and
// buffer size settings. When the console handles the interrupt, it will send
// cmdGetBuffer, which is when we actually read data from a buffer filled
// in the background.

CEXIMic::CEXIMic(Core::System& system, int index) : IEXIDevice(system), m_slot(index)
{
  m_microphone = std::make_unique<MicrophoneGameCube>(m_sampler);
  m_microphone->Initialize();
}

bool CEXIMic::IsPresent() const
{
  return true;
}

void CEXIMic::SetCS(int cs)
{
  if (cs)  // not-selected to selected
    m_position = 0;
  // Doesn't appear to do anything we care about
  // else if (command == cmdReset)
}

void CEXIMic::UpdateNextInterruptTicks()
{
  const int diff =
      (m_system.GetSystemTimers().GetTicksPerSecond() / m_sample_rate) * m_buff_size_samples;
  m_next_int_ticks = m_system.GetCoreTiming().GetTicks() + diff;
  m_system.GetExpansionInterface().ScheduleUpdateInterrupts(CoreTiming::FromThread::CPU, diff);
}

bool CEXIMic::IsInterruptSet()
{
  if (m_next_int_ticks == 0 || m_system.GetCoreTiming().GetTicks() < m_next_int_ticks)
    return false;

  if (m_sampler.IsSampleOn())
    UpdateNextInterruptTicks();
  else
    m_next_int_ticks = 0;

  return true;
}

void CEXIMic::TransferByte(u8& byte)
{
  if (m_position == 0)
  {
    m_command = byte;  // first byte is command
    byte = 0xFF;       // would be tristate, but we don't care.
    m_position++;
    return;
  }

  const int pos = m_position - 1;

  switch (m_command)
  {
  case cmdID:
    byte = EXI_ID[pos];
    break;

  case cmdGetStatus:
  {
    if (pos >= 2)
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "GetStatus will overflow, pos={}", pos);
    if (static_cast<MicrophoneGameCube*>(m_microphone.get())->GetOverflowBit())
    {
      m_sampler.status.buff_ovrflw = 1;
    }

    if (pos == 0)
      m_sampler.status.button = Pad::GetMicButton(m_slot);

    const u8* ptr = reinterpret_cast<u8*>(&m_sampler.status);
    ptr += pos ^ 1;  // byteswap
    byte = *ptr;

    if (pos == 1)
      m_sampler.status.buff_ovrflw = 0;
  }
  break;

  case cmdSetStatus:
  {
    if (pos >= 2)
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "SetStatus will overflow, pos={}", pos);
    const bool was_active = m_sampler.IsSampleOn();

    u8* ptr = reinterpret_cast<u8*>(&m_sampler.status);
    ptr += pos ^ 1;  // byteswap
    *ptr = byte;

    // safe to do since these can only be entered if both bytes of status have been written
    if (!was_active && m_sampler.IsSampleOn())
    {
      m_sample_rate = RATE_BASE << m_sampler.status.sample_rate;
      m_buff_size = RING_BASE << m_sampler.status.buff_size;
      m_buff_size_samples = m_buff_size / SAMPLE_SIZE;

      UpdateNextInterruptTicks();

      INFO_LOG_FMT(EXPANSIONINTERFACE, "Microphone sampling on (rate={}, samples={})",
                   m_sample_rate, m_buff_size_samples);
      m_microphone->SetSamplingRate(m_sample_rate);
    }
    else if (was_active && !m_sampler.IsSampleOn())
    {
      // If IsSampleOn() is false the DataCallback will skip frames.
      //
      // TODO: Is it worth to stop and destroy the stream each time?
      //
      // StreamStop();
    }
  }
  break;

  case cmdGetBuffer:
  {
    if (m_ring_pos == 0)
    {
      const u32 buff_size = static_cast<u32>(m_ring_buffer.size());
      if (m_buff_size > buff_size)
      {
        ERROR_LOG_FMT(EXPANSIONINTERFACE, "size={} larger than the ring_buffer's={}", m_buff_size,
                      buff_size);
      }
      m_ring_size = m_microphone->ReadIntoBuffer(m_ring_buffer.data(), buff_size);
    }

    if (m_ring_size == 0)
    {
      WARN_LOG_FMT(EXPANSIONINTERFACE, "Microphone buffer was empty");
      byte = 0;
    }
    else
    {
      byte = m_ring_buffer[m_ring_pos];
      m_ring_pos = (m_ring_pos + 1) % m_ring_size;
    }
  }
  break;

  default:
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "EXI MIC: unknown command byte {:02x}", m_command);
    break;
  }

  m_position++;
}
}  // namespace ExpansionInterface
