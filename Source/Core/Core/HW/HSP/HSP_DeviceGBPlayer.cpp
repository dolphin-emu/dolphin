// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/HSP/HSP_DeviceGBPlayer.h"

#include <cstring>

#if defined(HAS_LIBMGBA)
#include "Core/HW/GBACore.h"

#include <mgba/internal/gba/gba.h>
#endif

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "Core/Config/MainSettings.h"
#include "Core/CoreTiming.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

namespace
{

enum class GBPRegister : u8
{
  Test = 0x10,
  Video = 0x11,
  Control = 0x14,
  SIOControl = 0x15,
  Audio = 0x18,
  SIOData = 0x19,
  Keypad = 0x1c,
  IRQ = 0x1d,
};

constexpr u16 IRQ_ASSERTED = 0x8000;

constexpr u8 CONTROL_CART_IS_GB = 0x01;
constexpr u8 CONTROL_CART_INSERTED = 0x02;
constexpr u8 CONTROL_3V = 0x04;
constexpr u8 CONTROL_5V = 0x08;
constexpr u8 CONTROL_MASK_IRQ = 0x10;
// constexpr u8 CONTROL_SLEEP = 0x20;
// constexpr u8 CONTROL_LINK_CABLE = 0x40;
// constexpr u8 CONTROL_LINK_ENABLE = 0x80;

constexpr u32 AV_ADDRESS_MASK = 0xfe0;

class CGBPlayer_Dummy final : public HSP::IGBPlayer
{
public:
  using IGBPlayer::IGBPlayer;

  void Reset() override {}
  void Stop() override {}

  bool IsLoaded() const override { return false; }
  bool IsGBA() const override { return false; }

  void SetKeys(u16 keys) override {}

  u8 ReadSIOControl() override { return 0; }
  void WriteSIOControl(u8) override {}

  u32 ReadSIOData() override { return 0; }
  void WriteSIOData(u32) override {}

  void DoState(PointerWrap& p) override {}
};

}  // namespace

namespace HSP
{

enum class CHSPDevice_GBPlayer::IRQ : int
{
  Link = 0,
  GamePak = 1,
  Sleep = 2,
  Serial = 3,
  Video = 4,
  Audio = 5,
};

#if defined(HAS_LIBMGBA)

class CGBPlayer_mGBA final : public IGBPlayer
{
public:
  CGBPlayer_mGBA(Core::System&, CHSPDevice_GBPlayer*);

  CGBPlayer_mGBA(const CGBPlayer_mGBA&) = delete;
  CGBPlayer_mGBA(CGBPlayer_mGBA&&) = delete;
  CGBPlayer_mGBA& operator=(const CGBPlayer_mGBA&) = delete;
  CGBPlayer_mGBA& operator=(CGBPlayer_mGBA&&) = delete;

  void Reset() override;
  void Stop() override;

  bool IsLoaded() const override;
  bool IsGBA() const override;

  void SetKeys(u16 keys) override;

  u8 ReadSIOControl() override;
  void WriteSIOControl(u8) override;

  u32 ReadSIOData() override;
  void WriteSIOData(u32) override;

  void DoState(PointerWrap& p) override;

private:
  static constexpr std::size_t AUDIO_CHANNEL_COUNT = 2;

  HW::GBA::Core m_gba_core;

  struct AVStream : mAVStream
  {
    CGBPlayer_mGBA* player;
  } m_av_stream{};

  // A 4096 Hz event.
  CoreTiming::EventType* m_update_event = nullptr;

  void HandleUpdateEvent(s64 cycles_late);

  void UpdateAudio();
  void UpdateVideoAndScheduleNextUpdate(s64 cycles_late);

  // Prepare data in the AV_REGION_SIZE-sized buffers of CHSPDevice_GBPlayer.
  // IRQs trigger the game to read it.
  void PrepareScanlineData();
  void PrepareAudioData();

  // Read mGBA's sample buffer into our below PWM audio buffer.
  void ProcessAudioBuffer();

  std::array<u8, AV_REGION_SIZE * 4> m_audio_buffer{};
  u16 m_audio_buffer_read_pos = 0;
  u16 m_audio_buffer_write_pos = 0;

  // Used to calculate event times that don't drift.
  u16 m_audio_irq_phase = 0;

  // Set to 0 by mGBA thread upon new frame.
  u16 m_current_scanline_index = GBA_VIDEO_VERTICAL_PIXELS;

  s64 m_frame_start_ticks = 0;

  // GBA button input.
  u16 m_keys = 0;

  u16 m_audio_l_remainder = 0;
  u16 m_audio_r_remainder = 0;

  // Based on the sample rate from mGBA.
  u8 m_bits_per_sample = 9;
};

CGBPlayer_mGBA::CGBPlayer_mGBA(Core::System& system, CHSPDevice_GBPlayer* player)
    : IGBPlayer(system, player), m_gba_core{m_system, Config::GBPLAYER_GBA_INDEX}
{
  auto& core_timing = m_system.GetCoreTiming();

  m_update_event =
      core_timing.RegisterEvent("GBPmGBAUpdate", [this](Core::System&, u64, s64 cycles_late) {
        HandleUpdateEvent(cycles_late);
      });

  m_av_stream.player = this;

  m_av_stream.audioRateChanged = [](mAVStream* ptr, unsigned rate) {
    auto* const stream = static_cast<AVStream*>(ptr);
    auto* const player = stream->player;
    player->ProcessAudioBuffer();

    INFO_LOG_FMT(HSP, "GBPlayer: Audio rate changed: {}", rate);

    player->m_bits_per_sample = 24 - MathUtil::IntLog2(rate);
  };
  m_av_stream.postAudioBuffer = [](mAVStream* ptr, struct mAudioBuffer*) {
    auto* const stream = static_cast<AVStream*>(ptr);
    stream->player->ProcessAudioBuffer();
  };
}

void CGBPlayer_mGBA::Reset()
{
  Stop();

  m_gba_core.Start(m_system.GetCoreTiming().GetTicks());

  // Potentially BIOS or ROM could not be loaded.
  if (!m_gba_core.IsStarted())
    return;

  auto* const core = m_gba_core.GetCore();
  core->setAVStream(core, &m_av_stream);

  mCoreCallbacks callbacks{.context = this};

  callbacks.videoFrameEnded = [](void* context) {
    auto* const player = static_cast<CGBPlayer_mGBA*>(context);
    // Signal new frame to the next `Update` invocation.
    player->m_current_scanline_index = 0;
  };

  core->addCoreCallbacks(core, &callbacks);

  // Start the periodic updates.
  UpdateVideoAndScheduleNextUpdate(0);
}

void CGBPlayer_mGBA::Stop()
{
  if (!m_gba_core.IsStarted())
    return;

  m_gba_core.Stop();

  auto& core_timing = m_system.GetCoreTiming();

  core_timing.RemoveEvent(m_update_event);

  m_audio_buffer_read_pos = 0;
  m_audio_buffer_write_pos = 0;

  m_audio_irq_phase = 0;

  m_current_scanline_index = GBA_VIDEO_VERTICAL_PIXELS;
  m_frame_start_ticks = 0;

  m_keys = 0;

  m_audio_l_remainder = 0;
  m_audio_r_remainder = 0;

  m_bits_per_sample = 9;
}

bool CGBPlayer_mGBA::IsLoaded() const
{
  return true;
}

bool CGBPlayer_mGBA::IsGBA() const
{
  return m_gba_core.IsStarted() && m_gba_core.GetPlatform() == mPLATFORM_GBA;
}

void CGBPlayer_mGBA::PrepareScanlineData()
{
  const std::span video_buffer = m_gba_core.GetVideoBuffer();
  const std::span scanline_data = m_player->GetScanlineData();

  constexpr u32 scanline_count = 4;

  const u32* color_ptr =
      video_buffer.data() + (m_current_scanline_index * GBA_VIDEO_HORIZONTAL_PIXELS);

  for (u32 i = 0; i != GBA_VIDEO_HORIZONTAL_PIXELS * scanline_count; ++i)
  {
    const u32 color = *(color_ptr++);
    scanline_data[i] = M_RGB8_TO_RGB5(color);
  }

  if (m_current_scanline_index == 0)
    scanline_data[0] |= 0x8000u;

  m_current_scanline_index += scanline_count;
}

void CGBPlayer_mGBA::PrepareAudioData()
{
  const std::span audio_data = m_player->GetAudioData();

  // Read from the circular buffer.
  const auto count = std::min(m_audio_buffer.size() - m_audio_buffer_read_pos, audio_data.size());
  std::copy_n(m_audio_buffer.data() + m_audio_buffer_read_pos, count, audio_data.data());
  std::copy_n(m_audio_buffer.data(), audio_data.size() - count, audio_data.data() + count);

  m_audio_buffer_read_pos += u16(audio_data.size());
  m_audio_buffer_read_pos %= m_audio_buffer.size();
}

// Takes 5 bits from a 16-bit PCM sample and converts them into 32 bits of PWM.
//
// The 11 bits that don't get converted are fed into the remainder, which should be used as an
// input to the next invocation of this function to average out quantization errors over time.
//
// Unlike GBI, the Game Boy Player disc is picky about the PWM data.
// Every 32 bit sequence must have any 1 bits be contiguous and leading.
static constexpr u32 SampleToPWM(u16 value, u16* remainder)
{
  const u32 x = value + *remainder;
  const u32 y = x >> 11;
  *remainder = static_cast<u16>(x - (y << 11));
  return u32(0xffff'ffff'0000'0000ull >> y);
}

void CGBPlayer_mGBA::ProcessAudioBuffer()
{
  auto* const audio_buffer = m_gba_core.GetAudioBuffer();

  const u32 pwm_words_per_sample = 1u << (m_bits_per_sample - 5u);

  while (true)
  {
    std::array<std::array<s16, AUDIO_CHANNEL_COUNT>, 256> buffer;
    const auto read_count = mAudioBufferRead(audio_buffer, buffer.data()->data(), buffer.size());

    if (read_count == 0)
      break;

    for (const auto [l, r] : std::span{buffer}.first(read_count))
    {
      const u16 l_unsigned = l + 0x8000;
      const u16 r_unsigned = r + 0x8000;

      for (u32 i = 0; i != pwm_words_per_sample; ++i)
      {
        u32 l_pwm = SampleToPWM(l_unsigned, &m_audio_l_remainder);
        u32 r_pwm = SampleToPWM(r_unsigned, &m_audio_r_remainder);

        for (u32 j = 0; j != sizeof(u32); ++j)
        {
          m_audio_buffer[m_audio_buffer_write_pos++] = l_pwm >> 24;
          m_audio_buffer[m_audio_buffer_write_pos++] = r_pwm >> 24;
          l_pwm <<= 8;
          r_pwm <<= 8;
        }
      }

      m_audio_buffer_write_pos %= m_audio_buffer.size();
    }
  }
}

void CGBPlayer_mGBA::SetKeys(u16 keys)
{
  m_keys = keys;
}

void CGBPlayer_mGBA::DoState(PointerWrap& p)
{
  m_gba_core.DoState(p);

  p.Do(m_audio_irq_phase);

  p.Do(m_current_scanline_index);
  p.Do(m_frame_start_ticks);

  p.Do(m_keys);

  p.Do(m_audio_l_remainder);
  p.Do(m_audio_r_remainder);

  p.Do(m_audio_buffer);
  p.Do(m_audio_buffer_read_pos);
  p.Do(m_audio_buffer_write_pos);

  p.Do(m_bits_per_sample);
}

void CGBPlayer_mGBA::HandleUpdateEvent(s64 cycles_late)
{
  m_gba_core.Flush();
  UpdateAudio();
  UpdateVideoAndScheduleNextUpdate(cycles_late);
}

void CGBPlayer_mGBA::UpdateAudio()
{
  ProcessAudioBuffer();

  const auto audio_buffered =
      (m_audio_buffer_write_pos - m_audio_buffer_read_pos + m_audio_buffer.size()) %
      m_audio_buffer.size();

  if (audio_buffered < AV_REGION_SIZE)
  {
    // Don't IRQ if the audio runs behind somehow.
    // This shouldn't happen as long as mGBA runs for an appropriate number of cycles.
    WARN_LOG_FMT(HSP, "GBPlayer: Audio buffer underrun: {} < {}", audio_buffered, AV_REGION_SIZE);
    return;
  }

  PrepareAudioData();
  m_player->AssertIRQ(CHSPDevice_GBPlayer::IRQ::Audio);
}

void CGBPlayer_mGBA::UpdateVideoAndScheduleNextUpdate(s64 cycles_late)
{
  const s64 ticks_per_second = m_system.GetSystemTimers().GetTicksPerSecond();

  auto& core_timing = m_system.GetCoreTiming();
  const s64 ticks = core_timing.GetTicks();

  // mGBA signaled that it has a new frame.
  if (m_current_scanline_index == 0)
  {
    m_frame_start_ticks = ticks;
  }

  // Prepare data and set video IRQ every four scanlines.
  if (m_current_scanline_index < GBA_VIDEO_VERTICAL_PIXELS)
  {
    const s64 current_frame_ticks = ticks - m_frame_start_ticks;
    const u32 current_scanline_gba_ticks = VIDEO_HORIZONTAL_LENGTH * m_current_scanline_index;

    if (current_frame_ticks * GBA_ARM7TDMI_FREQUENCY >=
        current_scanline_gba_ticks * ticks_per_second)
    {
      PrepareScanlineData();
      m_player->AssertIRQ(CHSPDevice_GBPlayer::IRQ::Video);
    }
  }

  constexpr u32 audio_irq_freq =
      GBA_ARM7TDMI_FREQUENCY * AUDIO_CHANNEL_COUNT / CHAR_BIT / AV_REGION_SIZE;

  // Calculate a non-drifting time for the next update.
  // Note: Our video IRQs use audio IRQ timing with some jitter.
  // Sending separately timed video IRQs breaks the game.
  // I think the IRQs can't being cleared fast enough.

  const s64 this_irq_ticks = ticks_per_second * m_audio_irq_phase / audio_irq_freq;

  ++m_audio_irq_phase;
  const s64 next_irq_ticks = ticks_per_second * m_audio_irq_phase / audio_irq_freq;

  m_audio_irq_phase %= audio_irq_freq;

  const auto rel_ticks = next_irq_ticks - this_irq_ticks;

  // Run mGBA and schedule the next followup.
  m_gba_core.SyncJoybus(ticks + rel_ticks, m_keys);
  core_timing.ScheduleEvent(rel_ticks - cycles_late, m_update_event);
}

u8 CGBPlayer_mGBA::ReadSIOControl()
{
  DEBUG_LOG_FMT(HSP, "GBPlayer: SIOControl Read");

  return 0;
}

void CGBPlayer_mGBA::WriteSIOControl(u8 value)
{
  DEBUG_LOG_FMT(HSP, "GBPlayer: SIOControl Write: 0x{:02x}", value);
}

u32 CGBPlayer_mGBA::ReadSIOData()
{
  DEBUG_LOG_FMT(HSP, "GBPlayer: SIOData Read");

  return 0;
}

void CGBPlayer_mGBA::WriteSIOData(u32 value)
{
  DEBUG_LOG_FMT(HSP, "GBPlayer: SIOData Write: 0x{:08x}", value);
}

#endif

CHSPDevice_GBPlayer::CHSPDevice_GBPlayer(Core::System& system) : m_system{system}
{
#if defined(HAS_LIBMGBA)
  m_gbp = std::make_unique<CGBPlayer_mGBA>(m_system, this);
#else
  m_gbp = std::make_unique<CGBPlayer_Dummy>(m_system, this);
#endif
}

void CHSPDevice_GBPlayer::Read(u32 address, std::span<u8, TRANSFER_SIZE> data)
{
  switch (GBPRegister(address >> 20))
  {
  case GBPRegister::Test:
  {
    std::ranges::copy(m_test, data.data());
    break;
  }
  case GBPRegister::Control:
  {
    if (m_gbp->IsLoaded())
    {
      m_control |= CONTROL_CART_INSERTED;
      if (m_gbp->IsGBA())
        m_control &= ~CONTROL_CART_IS_GB;
      else
        m_control |= CONTROL_CART_IS_GB;
    }
    else
    {
      m_control &= ~(CONTROL_CART_IS_GB | CONTROL_CART_INSERTED);
    }

    std::ranges::fill(data, m_control);
    break;
  }
  case GBPRegister::IRQ:
  {
    u32 value = m_irq;
    value |= (m_irq & 0xff00) * 0x00010100u;
    value &= 0xffff7fffu;
    value = Common::swap32(value);
    for (std::size_t i = 0; i != data.size(); i += sizeof(value))
    {
      std::memcpy(data.data() + i, &value, sizeof(value));
    }
    break;
  }
  case GBPRegister::Video:
  {
    u32 scanlines_pos = (address & AV_ADDRESS_MASK) / sizeof(u32);
    for (u32 i = 0; i != data.size(); i += sizeof(u32))
    {
      const u16 color = m_scanlines[scanlines_pos++];
      data[i + 0] = data[i + 1] = u8(color >> 8);
      data[i + 2] = data[i + 3] = u8(color);
    }
    break;
  }
  case GBPRegister::Audio:
  {
    u32 audio_pos = (address & AV_ADDRESS_MASK) / sizeof(u32);
    for (u32 i = 0; i != data.size(); i += sizeof(u32))
    {
      std::memset(data.data() + i, m_audio[audio_pos++], sizeof(u32));
    }
    break;
  }
  case GBPRegister::SIOControl:
  {
    const u8 value = m_gbp->ReadSIOControl();
    std::ranges::fill(data, value);
    break;
  }
  case GBPRegister::SIOData:
  {
    const u32 value = m_gbp->ReadSIOData();
    for (std::size_t i = 0; i != data.size(); i += sizeof(value))
    {
      std::memcpy(data.data() + i, &value, sizeof(value));
    }
    break;
  }
  default:
  {
    WARN_LOG_FMT(HSP, "GBPlayer: Unknown read from 0x{:08x}", address);
    break;
  }
  }
}

void CHSPDevice_GBPlayer::Write(u32 address, std::span<const u8, TRANSFER_SIZE> data)
{
  switch (GBPRegister(address >> 20))
  {
  case GBPRegister::Test:
  {
    u8* dest = m_test.data();
    for (u8 value : data)
      *(dest++) = value ^ 0xffu;
    break;
  }
  case GBPRegister::Control:
  {
    const u8 value = data[0x1f];

    if (!(m_control & (CONTROL_3V | CONTROL_5V)) && (value & (CONTROL_3V | CONTROL_5V)))
    {
      INFO_LOG_FMT(HSP, "GBPlayer: Reset");
      m_gbp->Reset();
    }

    if ((m_control & (CONTROL_3V | CONTROL_5V)) && !(value & (CONTROL_3V | CONTROL_5V)))
    {
      INFO_LOG_FMT(HSP, "GBPlayer: Stop");
      m_gbp->Stop();
    }

    m_control = value & 0xFC;
    UpdateInterrupts();
    break;
  }
  case GBPRegister::IRQ:
  {
    const u16 value = Common::swap16(data.data() + 0x1e);
    m_irq &= u16(~value);
    UpdateInterrupts();
    break;
  }
  case GBPRegister::Keypad:
  {
    const u8 value_hi = data[0x1e];  // L/R triggers (need to be flipped).
    const u8 value_lo = data[0x1f];  // All other buttons.
    m_gbp->SetKeys(u16(((value_hi & 0x01u) << 9) | ((value_hi & 0x02u) << 7) | value_lo));
    break;
  }
  case GBPRegister::SIOControl:
  {
    const u8 value = data[0x1f];
    m_gbp->WriteSIOControl(value);
    break;
  }
  case GBPRegister::SIOData:
  {
    const u32 value = Common::swap32(data.data() + 0x1c);
    m_gbp->WriteSIOData(value);
    break;
  }
  default:
  {
    WARN_LOG_FMT(HSP, "GBPlayer: Unknown write to 0x{:08x}", address);
    break;
  }
  }
}

void CHSPDevice_GBPlayer::DoState(PointerWrap& p)
{
  // Fail when trying to load a state with a different HAS_LIBMGBA value.
#if defined(HAS_LIBMGBA)
  p.DoMarker("Have mGBA", 1);
#else
  p.DoMarker("Have mGBA", 0);
#endif

  p.Do(m_test);
  p.Do(m_control);
  p.Do(m_irq);

  p.Do(m_scanlines);
  p.Do(m_audio);

  m_gbp->DoState(p);
}

void CHSPDevice_GBPlayer::AssertIRQ(IRQ irq)
{
  // The 6 IRQ types set the even bits: 0x555.
  // FYI: GBPlayer software appears to use the odd bits to mask the even bits.
  const u32 irq_bit_value = 1u << (2u * u32(irq));

  if ((m_irq & irq_bit_value) != 0)
    WARN_LOG_FMT(HSP, "GBPlayer: IRQ {} is already set.", u32(irq));

  m_irq |= irq_bit_value | IRQ_ASSERTED;
  UpdateInterrupts();
}

void CHSPDevice_GBPlayer::UpdateInterrupts()
{
  const bool set_interrupt = ((m_control & CONTROL_MASK_IRQ) == 0) && ((m_irq & IRQ_ASSERTED) != 0);
  m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_HSP, set_interrupt);
}

IGBPlayer::IGBPlayer(Core::System& system, CHSPDevice_GBPlayer* player)
    : m_system{system}, m_player{player}
{
}

IGBPlayer::~IGBPlayer() = default;

}  // namespace HSP
