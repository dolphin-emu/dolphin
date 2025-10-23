// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Speaker.h"

#include <cassert>

#include "AudioCommon/AudioCommon.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/System.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace WiimoteEmu
{
// Yamaha ADPCM decoder code based on The ffmpeg Project (Copyright (s) 2001-2003)

static const s32 yamaha_difflookup[] = {1,  3,  5,  7,  9,  11,  13,  15,
                                        -1, -3, -5, -7, -9, -11, -13, -15};

static const s32 yamaha_indexscale[] = {230, 230, 230, 230, 307, 409, 512, 614,
                                        230, 230, 230, 230, 307, 409, 512, 614};

static s16 av_clip16(s32 a)
{
  if ((a + 32768) & ~65535)
    return (a >> 31) ^ 32767;
  else
    return a;
}

static s32 av_clip(s32 a, s32 amin, s32 amax)
{
  if (a < amin)
    return amin;
  else if (a > amax)
    return amax;
  else
    return a;
}

static s16 adpcm_yamaha_expand_nibble(ADPCMState& s, u8 nibble)
{
  s.predictor += (s.step * yamaha_difflookup[nibble]) / 8;
  s.predictor = av_clip16(s.predictor);
  s.step = (s.step * yamaha_indexscale[nibble]) >> 8;
  s.step = av_clip(s.step, 127, 24576);
  return s.predictor;
}

static constexpr auto ExpandS8ToS16(s8 sample)
{
  return s16(sample * 0x100);
}

void SpeakerLogic::ProcessSpeakerData(std::span<const u8> data)
{
  // There seem to be multiple flags that can skip input entirely.
  if ((m_register_data.speaker_flags & SPEAKER_STOP_BIT) != 0x00)
    return;
  if ((m_register_data.audio_input_enable & 0x01) == 0x00)
    return;

  // Potentially 40 resulting samples.
  std::array<s16, WiimoteCommon::OutputReportSpeakerData::DATA_SIZE * 2> samples;
  assert(data.size() * 2 <= samples.size());

  std::size_t sample_count = 0;

  const u8 effective_audio_format = m_register_data.audio_format & 0xe0;
  switch (effective_audio_format)
  {
    // 4bit Yamaha ADPCM (same as dreamcast)
    // Games only use this.
  case 0x00:
    for (u8 value : data)
    {
      samples[sample_count++] = adpcm_yamaha_expand_nibble(m_adpcm_state, value >> 4);
      samples[sample_count++] = adpcm_yamaha_expand_nibble(m_adpcm_state, value & 0xf);
    }
    break;

    // s8 PCM
  case 0x40:
    for (u8 value : data)
    {
      samples[sample_count++] = ExpandS8ToS16(s8(value));
    }
    break;

    // s16le PCM (both of these?)
  case 0x60:
  case 0xe0:
    if ((data.size() % sizeof(s16)) != 0)
    {
      // I assume the real hardware properly buffers odd-sized writes ?
      ERROR_LOG_FMT(IOS_WIIMOTE, "Unhandled odd audio data size");
    }
    sample_count = data.size() / sizeof(s16);
    std::ranges::copy(data, Common::AsWritableU8Span(samples).data());
    break;

  default:
    ERROR_LOG_FMT(IOS_WIIMOTE, "Unknown audio format {:x}", m_register_data.audio_format);
    break;
  }

  if (sample_count == 0)
    return;

  m_register_data.decoder_flags |= DECODER_PROCESSED_DATA_BIT;

  // When output isn't enabled, we drop the samples after running data through the decoder.
  // I think the real hardware buffers the pre-decoded data and decodes/plays only when this is set.
  // And I think the above bit is not actually set until that happens.
  // We aren't emulating any of that buffering, though.
  if ((m_register_data.audio_playback_enable & 0x01) == 0x00)
    return;

  if (m_is_muted)
    return;

  // If the 0x01 bit is set here then no audio is produced.
  if ((m_register_data.audio_format & 0x01) != 0x00)
    return;

  // Despite wiibrew claims, the hardware volume can go all the way to 0xff.
  // But since games never set the volume beyond 0x7f we'll use that as the maximum.
  constexpr auto volume_divisor = 0x7f;

  if (m_register_data.volume > volume_divisor)
  {
    // We could potentially amplify the samples in this situation?
    DEBUG_LOG_FMT(IOS_WIIMOTE, "Wiimote volume is higher than suspected maximum.");
  }

  // SetWiimoteSpeakerVolume expects values from 0 to 255.
  // Multiply by 256, floor to int, and clamp to 255 for a uniformly mapped conversion.
  const double volume = float(m_register_data.volume) * 256.f / volume_divisor;

  // This used the "Constant Power Pan Law", but it is undesirable
  // if the pan is 0, and it implied that the loudness of a wiimote speaker
  // is equal to the loudness of your device speakers, which isn't true at all.
  // This way, if the pan is 0, it's like if it is not there.
  // We should play the samples from the wiimote at the native volume they came with,
  // because you can lower their volume from the Wii settings and because they are
  // already extremely low quality, so any additional quality loss isn't welcome.
  const auto speaker_pan = std::clamp(float(m_speaker_pan_setting.GetValue()) / 100, -1.f, 1.f);
  const u32 l_volume = std::min(u32(std::min(1.f - speaker_pan, 1.f) * volume), 255u);
  const u32 r_volume = std::min(u32(std::min(1.f + speaker_pan, 1.f) * volume), 255u);

  auto& system = Core::System::GetInstance();
  SoundStream* sound_stream = system.GetSoundStream();

  sound_stream->GetMixer()->SetWiimoteSpeakerVolume(l_volume, r_volume);
  sound_stream->GetMixer()->PushWiimoteSpeakerSamples(
      samples.data(), sample_count, Mixer::FIXED_SAMPLE_RATE_DIVIDEND / GetCurrentSampleRate());
}

u32 SpeakerLogic::GetCurrentSampleRate() const
{
  auto sr_divisor = m_register_data.sample_rate_divisor;
  if (sr_divisor == 0)
  {
    // Real hardware seems to interpret 0x000 as 0xfff.
    // i.e. ~183 samples per second, based buffer "readiness" timing.
    sr_divisor = std::numeric_limits<u16>::max();
  }

  return SAMPLE_RATE_DIVIDEND / sr_divisor;
}

void SpeakerLogic::Reset()
{
  m_register_data = {};

  m_register_data.decoder_flags |= DECODER_READY_FOR_DATA_BIT;

  m_adpcm_state = {};

  m_is_enabled = false;
  m_is_muted = false;
}

void SpeakerLogic::DoState(PointerWrap& p)
{
  p.Do(m_adpcm_state);
  p.Do(m_register_data);

  // FYI: `m_is_enabled` and `m_is_muted` are handled by the Wiimote class.
}

void SpeakerLogic::SetEnabled(bool enabled)
{
  m_is_enabled = enabled;
}

void SpeakerLogic::SetMuted(bool muted)
{
  m_is_muted = muted;
}

int SpeakerLogic::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  // FYI: Real hardware returns a stream of 0xff when reading from addr:0x00.
  // Reads at other addresses also return mostly 0xff except:
  //  addr:0x07 returns some decoder state flags.
  //  addr:0xff returns 0x00.
  // Games never read from this device so this isn't implemented.

  return RawRead(&m_register_data, addr, count, data_out);
}

int SpeakerLogic::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  // It seems writes while not "enabled" succeed but have no effect.
  if (!m_is_enabled)
    return count;

  if (addr == SPEAKER_DATA_OFFSET)
  {
    ProcessSpeakerData(std::span(data_in, count));
    return count;
  }

  // Allow writes to clear some bits.
  const auto prev_flags = std::exchange(m_register_data.decoder_flags, 0x00);
  RawWrite(&m_register_data, addr, count, data_in);
  const auto written_bits = std::exchange(m_register_data.decoder_flags, prev_flags);

  constexpr u8 CLEARABLE_BITS = DECODER_DROPPED_DATA_BIT | DECODER_PROCESSED_DATA_BIT;

  m_register_data.decoder_flags &= ~(written_bits & CLEARABLE_BITS);

  if ((written_bits & DECODER_RESET_BIT) != 0x00)
  {
    // The real hardware also sets bits 0x04 and 0x08 here if data was buffered.

    m_adpcm_state = {};
    DEBUG_LOG_FMT(IOS_WIIMOTE, "ADPCM decoder reset via i2c write.");
  }

  if ((m_register_data.speaker_flags & SPEAKER_DISABLE_PLAYBACK_BIT) != 0x00)
    Common::SetBit<0>(m_register_data.audio_playback_enable, false);

  return count;
}

}  // namespace WiimoteEmu
