// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/HSP/HSP_DeviceGBPlayer.h"

#include <cstring>
#include <ratio>

#if defined(HAS_LIBMGBA)
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/video.h>

#include "Core/HW/GBACore.h"
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
  SIO = 0x19,
  Keypad = 0x1c,
  IRQ = 0x1d,
};

constexpr u16 IRQ_ASSERTED = 0x8000;

constexpr u8 CONTROL_CART_DETECTED = 0x01;
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

  void ReadScanlines(std::span<u32, AV_REGION_SIZE> scanlines) override {}
  void ReadAudio(std::span<u8, AV_REGION_SIZE> audio) override {}

  void SetKeys(u16 keys) override {}

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

  void ReadScanlines(std::span<u32, AV_REGION_SIZE> scanlines) override;
  void ReadAudio(std::span<u8, AV_REGION_SIZE> audio) override;

  void SetKeys(u16 keys) override;

  void DoState(PointerWrap& p) override;

private:
  static constexpr std::size_t AUDIO_BUFFER_SIZE = 512;

  void UpdateAudio(s64 cycles_late);
  void UpdateVideo(u32 next_scanlines, s64 cycles_late);

  HW::GBA::Core m_gba_core;

  mAudioBuffer m_audio_buffer;
  mAudioResampler m_audio_resampler;

  CoreTiming::EventType* m_update_audio_event = nullptr;
  CoreTiming::EventType* m_update_video_event = nullptr;

  // These are used to calculate timings that don't drift.
  u32 m_video_irq_phase = 0;
  u16 m_audio_irq_phase = 0;

  u16 m_current_scanline_index = 0;
  u16 m_keys = 0;

  // Used to trigger a video IRQ during the next audio IRQ in UpdateAudio.
  // Separately timed video IRQs cause the AV stream to enter a bad state.
  // It's very fragile for some reason. This might be a CPU timing issue ?
  bool m_generate_video_irq = false;
};

CGBPlayer_mGBA::CGBPlayer_mGBA(Core::System& system, CHSPDevice_GBPlayer* player)
    : IGBPlayer(system, player), m_gba_core{m_system, Config::GBPLAYER_GBA_INDEX}
{
  auto& core_timing = m_system.GetCoreTiming();

  m_update_audio_event =
      core_timing.RegisterEvent("GBPmGBAUpdateAudio", [this](Core::System&, u64, s64 cycles_late) {
        UpdateAudio(cycles_late);
      });
  m_update_video_event = core_timing.RegisterEvent(
      "GBPmGBAUpdateVideo", [this](Core::System&, u64 user_data, s64 cycles_late) {
        UpdateVideo(u32(user_data), cycles_late);
      });

  mAudioBufferInit(&m_audio_buffer, AUDIO_BUFFER_SIZE, AUDIO_CHANNEL_COUNT);
  mAudioResamplerInit(&m_audio_resampler, mINTERPOLATOR_SINC);
  mAudioResamplerSetDestination(&m_audio_resampler, &m_audio_buffer, AUDIO_SAMPLE_RATE);

  // TODO: Does the real hardware boot on power up or wait for the GBPRegister::Control command ?
  Reset();
}

void CGBPlayer_mGBA::Reset()
{
  Stop();

  m_gba_core.Start(m_system.GetCoreTiming().GetTicks());

  // Potentially BIOS or ROM could not be loaded.
  if (!m_gba_core.IsStarted())
    return;

  auto& core_timing = m_system.GetCoreTiming();

  // Start audio/video updates after a delay. This timing isn't critical.
  const auto ticks_per_second = m_system.GetSystemTimers().GetTicksPerSecond();
  core_timing.ScheduleEvent(ticks_per_second / 50, m_update_audio_event);
  core_timing.ScheduleEvent(ticks_per_second / 50, m_update_video_event, 0);
}

void CGBPlayer_mGBA::Stop()
{
  if (!m_gba_core.IsStarted())
    return;

  auto& core_timing = m_system.GetCoreTiming();

  core_timing.RemoveEvent(m_update_audio_event);
  core_timing.RemoveEvent(m_update_video_event);

  m_video_irq_phase = 0;
  m_audio_irq_phase = 0;

  m_current_scanline_index = 0;
  m_keys = 0;
  m_generate_video_irq = false;

  m_gba_core.Stop();

  mAudioBufferClear(&m_audio_buffer);
}

bool CGBPlayer_mGBA::IsLoaded() const
{
  return true;
}

bool CGBPlayer_mGBA::IsGBA() const
{
  return m_gba_core.IsStarted() && m_gba_core.GetPlatform() == mPLATFORM_GBA;
}

void CGBPlayer_mGBA::ReadScanlines(std::span<u32, AV_REGION_SIZE> scanlines)
{
  DEBUG_LOG_FMT(HSP, "GBPlayer: ReadScanlines: {}", m_current_scanline_index);

  if (m_current_scanline_index >= GBA_VIDEO_VERTICAL_PIXELS)
    return;

  if (!m_gba_core.IsStarted())
    return;

  // Since the GBA core is idle, this is a convenient time to read and resample some audio.
  mAudioResamplerSetSource(&m_audio_resampler, m_gba_core.GetAudioBuffer(),
                           m_gba_core.GetAudioSampleRate(), true);
  mAudioResamplerProcess(&m_audio_resampler);

  const std::span video_buffer = m_gba_core.GetVideoBuffer();

  for (u32 i = 0; i != GBA_VIDEO_HORIZONTAL_PIXELS * 4; ++i)
  {
    const u32 color =
        M_RGB8_TO_RGB5(video_buffer[(m_current_scanline_index * GBA_VIDEO_HORIZONTAL_PIXELS) + i]);
    u32 c = color >> 8u;
    c |= (color & 0xffu) << 16u;
    scanlines[i] = c | (c << 8);  // Parity
  }

  if (m_current_scanline_index == 0)
    scanlines[0] |= 0x00008080;

  m_current_scanline_index += 4;

  if (m_current_scanline_index < GBA_VIDEO_VERTICAL_PIXELS)
    m_generate_video_irq = true;
}

// TODO: Explain the meaning of this audio sample processing.
static constexpr u8 SampleToBits(u16* value)
{
  const auto x = std::min<u32>(*value, 8);
  *value -= x;
  return u8(0xff00u >> x);
}

void CGBPlayer_mGBA::ReadAudio(std::span<u8, AV_REGION_SIZE> audio)
{
  std::array<std::array<s16, AUDIO_CHANNEL_COUNT>, AUDIO_READ_SIZE> buffer;
  const auto read_count = mAudioBufferRead(&m_audio_buffer, buffer.data()->data(), buffer.size());

  // Each s16 sample is converted to a u9 which is then converted to 64 u8 values.
  // This must have something to do with interpretation by the DSP ?
  // TODO: Explain things better.

  constexpr u32 out_bytes_per_sample = AV_REGION_SIZE / AUDIO_READ_SIZE / AUDIO_CHANNEL_COUNT;
  static_assert(out_bytes_per_sample == 64);

  auto out_it = audio.begin();
  for (auto [l, r] : std::span{buffer}.first(read_count))
  {
    auto l_9bit = u16(u32(l + 0x8000) >> 7u);
    auto r_9bit = u16(u32(r + 0x8000) >> 7u);

    for (u32 i = 0; i != out_bytes_per_sample; ++i)
    {
      *(out_it++) = SampleToBits(&l_9bit);
      *(out_it++) = SampleToBits(&r_9bit);
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

  p.Do(m_video_irq_phase);
  p.Do(m_audio_irq_phase);

  p.Do(m_current_scanline_index);
  p.Do(m_keys);
  p.Do(m_generate_video_irq);

  // Resampled audio buffer.
  p.DoArray(static_cast<u8*>(m_audio_buffer.data.data), u32(m_audio_buffer.data.capacity));
  p.Do(m_audio_buffer.data.size);
  p.DoPointer(m_audio_buffer.data.readPtr, m_audio_buffer.data.data);
  p.DoPointer(m_audio_buffer.data.writePtr, m_audio_buffer.data.data);
}

void CGBPlayer_mGBA::UpdateAudio(s64 cycles_late)
{
  m_player->AssertIRQ(CHSPDevice_GBPlayer::IRQ::Audio);

  if (m_generate_video_irq)
  {
    m_player->AssertIRQ(CHSPDevice_GBPlayer::IRQ::Video);
    m_generate_video_irq = false;
  }

  constexpr u32 audio_irq_freq = AUDIO_SAMPLE_RATE / AUDIO_READ_SIZE;

  const s64 ticks_per_second = m_system.GetSystemTimers().GetTicksPerSecond();

  // Calculate a non-drifting time for the next IRQ.
  const s64 this_irq_ticks = ticks_per_second * m_audio_irq_phase / audio_irq_freq;

  ++m_audio_irq_phase;
  const s64 next_irq_ticks = ticks_per_second * m_audio_irq_phase / audio_irq_freq;

  m_audio_irq_phase %= audio_irq_freq;

  m_system.GetCoreTiming().ScheduleEvent(next_irq_ticks - this_irq_ticks - cycles_late,
                                         m_update_audio_event);
}

void CGBPlayer_mGBA::UpdateVideo(u32 scanline_index, s64 cycles_late)
{
  // The ~59.7 GBA framerate.
  constexpr std::ratio<GBA_ARM7TDMI_FREQUENCY, VIDEO_TOTAL_LENGTH> gba_framerate;

  if (scanline_index == 0)
  {
    DEBUG_LOG_FMT(HSP, "GBPlayer: Frame start");

    m_current_scanline_index = 0;

    m_generate_video_irq = true;

    auto& core_timing = m_system.GetCoreTiming();

    const s64 ticks_per_second = m_system.GetSystemTimers().GetTicksPerSecond();

    // Schedule an event to execute the GBA on vblank.
    const s64 visible_scanlines_ticks = ticks_per_second * GBA_VIDEO_VERTICAL_PIXELS *
                                        VIDEO_HORIZONTAL_LENGTH / GBA_ARM7TDMI_FREQUENCY;
    core_timing.ScheduleEvent(visible_scanlines_ticks - cycles_late, m_update_video_event,
                              GBA_VIDEO_VERTICAL_PIXELS);

    // Calculate a non-drifting time for the start of the next frame.
    const s64 this_frame_ticks =
        ticks_per_second * m_video_irq_phase * gba_framerate.den / gba_framerate.num;

    ++m_video_irq_phase;
    const s64 next_frame_ticks =
        ticks_per_second * m_video_irq_phase * gba_framerate.den / gba_framerate.num;

    m_video_irq_phase %= gba_framerate.num;

    core_timing.ScheduleEvent(next_frame_ticks - this_frame_ticks - cycles_late,
                              m_update_video_event, 0);

    // Ensure the GBA frame emulation is complete.
    m_gba_core.Flush();
  }
  else if (scanline_index == GBA_VIDEO_VERTICAL_PIXELS)
  {
    DEBUG_LOG_FMT(HSP, "GBPlayer: Frame end");

    if (m_current_scanline_index != GBA_VIDEO_VERTICAL_PIXELS)
    {
      // This happens when the game doesn't read the entire frame in time.
      // Lowering the CPU clock override will cause this to happen repeatedly.
      WARN_LOG_FMT(HSP, "GBPlayer: Frame end while reading scanlines: {}",
                   m_current_scanline_index);
    }

    // Emulate a single frame during vblank.
    m_gba_core.RunFrame(m_keys);

    m_current_scanline_index = scanline_index;
  }
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
      if ((m_control & CONTROL_3V) && (m_gbp->IsGBA() || (m_control & CONTROL_5V)))
        m_control |= CONTROL_CART_DETECTED;
      else
        m_control &= ~CONTROL_CART_DETECTED;
    }
    else
    {
      m_control &= ~(CONTROL_CART_DETECTED | CONTROL_CART_INSERTED);
    }

    std::ranges::fill(data, m_control);
    break;
  }
  case GBPRegister::IRQ:
  {
    u32 value = m_irq;
    value |= (m_irq & 0xff00) * 0x0100'0001u;
    value &= 0x7fff'ffffu;

    for (std::size_t i = 0; i != data.size(); i += sizeof(value))
    {
      std::memcpy(data.data() + i, &value, sizeof(value));
    }
    break;
  }
  case GBPRegister::Video:
  {
    const u32 offset = address & AV_ADDRESS_MASK;
    if (offset == 0)
      m_gbp->ReadScanlines(m_scanlines);

    const u32 scanlines_pos = offset / 4;

    std::memcpy(data.data(), m_scanlines.data() + scanlines_pos, data.size());
    break;
  }
  case GBPRegister::Audio:
  {
    const u32 offset = address & AV_ADDRESS_MASK;
    if (offset == 0)
      m_gbp->ReadAudio(m_audio);

    u32 audio_pos = offset / 4;

    for (std::size_t i = 0; i != data.size(); i += sizeof(u32))
    {
      std::memset(data.data() + i, m_audio[audio_pos], sizeof(u32));
      ++audio_pos;
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

      m_scanlines.fill(0);
      m_audio.fill(0);
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
    const u16 value = Common::swap16(data.data() + 0x1e);
    m_gbp->SetKeys(value & 0x3FF);
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
