// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceMic.h"

#include <mutex>

#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/Config/MainSettings.h"
#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

namespace ExpansionInterface
{
bool GameCubeMicState::IsSampleOn() const
{
  return m_sampling_enabled.load(std::memory_order_relaxed);
}

bool GameCubeMicState::IsMuted() const
{
  return false;  // Not applicable
}

u32 GameCubeMicState::GetDefaultSamplingRate() const
{
  return DEFAULT_SAMPLING_RATE << status.sample_rate;
}

void GameCubeMicState::SyncSamplingState()
{
  m_sampling_enabled.store(status.is_active, std::memory_order_relaxed);
}

u32 GameCubeMicState::GetSampleRate() const
{
  return DEFAULT_SAMPLING_RATE << status.sample_rate;
}

u32 GameCubeMicState::GetBufferSize() const
{
  return RING_BASE << status.buff_size;
}

u32 GameCubeMicState::GetBufferSizeSamples() const
{
  return GetBufferSize() / SAMPLE_SIZE;
}

namespace
{
class MicrophoneGameCube : public AudioCommon::Microphone
{
public:
  explicit MicrophoneGameCube(const GameCubeMicState& sampler, int index)
      : Microphone(sampler, fmt::format("GameCube Microphone {}", index)), m_sampler(sampler),
        m_buff_size_samples(sampler.GetBufferSizeSamples()), m_index(index)
  {
    ASSERT_MSG(EXPANSIONINTERFACE, index == 0 || index == 1, "Invalid GameCube microphone port {}",
               index);
  }

  bool GetOverflowBit() { return m_overflow_bit_set.exchange(false, std::memory_order_relaxed); }

private:
#ifdef HAVE_CUBEB
  std::string GetInputDeviceId() const override
  {
    return Config::Get(Config::MAIN_EXI_MIC_MICROPHONE[m_index]);
  }

  std::string GetCubebStreamName() const override
  {
    return fmt::format("Dolphin Emulated GameCube Microphone {}", m_index);
  }

  s16 GetVolumeModifier() const override
  {
    return Config::Get(Config::MAIN_EXI_MIC_VOLUME_MODIFIER[m_index]);
  }

  bool AreSamplesByteSwapped() const override { return true; }

  void OnOverflow() override
  {
    m_samples_avail = 0;
    m_overflow_bit_set.store(true, std::memory_order_relaxed);
  }
#endif

  bool IsMicrophoneMuted() const override
  {
    return Config::Get(Config::MAIN_EXI_MIC_MUTED[m_index]);
  }

  u32 GetBufferSizeSamples() const override { return m_buff_size_samples; }

  u32 GetStreamSize() const override { return m_buff_size_samples * 500; }

  const GameCubeMicState& m_sampler;
  const u32 m_buff_size_samples;
  const int m_index;
  std::atomic_bool m_overflow_bit_set{false};
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
  m_microphone = std::make_unique<MicrophoneGameCube>(m_sampler, index);
  m_microphone->Initialize();
}

CEXIMic::~CEXIMic() = default;

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

  const u32 pos = m_position - 1;

  switch (m_command)
  {
  case cmdID:
    if (pos >= EXI_ID.size())
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "ID command will overflow, pos={}", pos);
      break;
    }
    byte = EXI_ID[pos];
    break;

  case cmdGetStatus:
    if (pos >= sizeof(m_sampler.status))
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "GetStatus command will overflow, pos={}", pos);
      break;
    }
    if (pos == 0)
    {
      m_sampler.status.button = Pad::GetMicButton(m_slot);
      if (m_microphone && static_cast<MicrophoneGameCube*>(m_microphone.get())->GetOverflowBit())
      {
        m_sampler.status.buff_ovrflw = 1;
      }
    }

    byte = Common::BitCastPtr<u8>(&m_sampler.status)[pos ^ 1];

    if (pos == 1)
      m_sampler.status.buff_ovrflw = 0;
    break;

  case cmdSetStatus:
  {
    if (pos >= sizeof(m_sampler.status))
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "SetStatus command will overflow, pos={}", pos);
      break;
    }
    const bool was_active = m_sampler.IsSampleOn();
    Common::BitCastPtr<u8>(&m_sampler.status)[pos ^ 1] = byte;
    m_sampler.SyncSamplingState();

    // safe to do since these can only be entered if both bytes of status have been written
    if (!was_active && m_sampler.IsSampleOn())
    {
      m_sample_rate = m_sampler.GetSampleRate();
      m_buff_size = m_sampler.GetBufferSize();
      m_buff_size_samples = m_sampler.GetBufferSizeSamples();

      UpdateNextInterruptTicks();

      INFO_LOG_FMT(EXPANSIONINTERFACE, "Microphone sampling on (rate={}, samples={})",
                   m_sample_rate, m_buff_size_samples);
      m_microphone = std::make_unique<MicrophoneGameCube>(m_sampler, m_slot);
      m_microphone->Initialize();
    }
    else if (was_active && !m_sampler.IsSampleOn())
    {
      // If IsSampleOn() is false the DataCallback will skip frames.
      //
      // TODO: Is it worth to stop and destroy the stream each time?
      m_microphone.reset();
    }
  }
  break;

  case cmdGetBuffer:
  {
    if (m_ring_pos == 0 && m_microphone)
      m_microphone->ReadIntoBuffer(m_ring_buffer.data(), m_buff_size);

    byte = m_ring_buffer[m_ring_pos];
    m_ring_pos = (m_ring_pos + 1) % m_buff_size;
  }
  break;

  default:
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "EXI MIC: unknown command byte {:02x}", m_command);
    break;
  }

  m_position++;
}
}  // namespace ExpansionInterface
