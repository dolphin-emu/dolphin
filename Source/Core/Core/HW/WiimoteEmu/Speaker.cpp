// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Speaker.h"

#include <memory>

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

void SpeakerLogic::SpeakerData(const u8* data, int length, float speaker_pan)
{
  // TODO: should we still process samples for the decoder state?
  if (!m_speaker_enabled)
    return;

  if (reg_data.sample_rate == 0 || length == 0)
    return;

  // Even if volume is zero we process samples to maintain proper decoder state.

  // TODO consider using static max size instead of new
  std::unique_ptr<s16[]> samples(new s16[length * 2]);

  unsigned int sample_rate_dividend, sample_length;
  u8 volume_divisor;

  if (reg_data.format == SpeakerLogic::DATA_FORMAT_PCM)
  {
    // 8 bit PCM
    for (int i = 0; i < length; ++i)
    {
      samples[i] = ((s16)(s8)data[i]) * 0x100;
    }

    // Following details from http://wiibrew.org/wiki/Wiimote#Speaker
    sample_rate_dividend = 12000000;
    volume_divisor = 0xff;
    sample_length = (unsigned int)length;
  }
  else if (reg_data.format == SpeakerLogic::DATA_FORMAT_ADPCM)
  {
    // 4 bit Yamaha ADPCM (same as dreamcast)
    for (int i = 0; i < length; ++i)
    {
      samples[i * 2] = adpcm_yamaha_expand_nibble(adpcm_state, (data[i] >> 4) & 0xf);
      samples[i * 2 + 1] = adpcm_yamaha_expand_nibble(adpcm_state, data[i] & 0xf);
    }

    // Following details from http://wiibrew.org/wiki/Wiimote#Speaker
    sample_rate_dividend = 6000000;
    volume_divisor = 0x7F;
    sample_length = (unsigned int)length * 2;
  }
  else
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Unknown speaker format {:x}", reg_data.format);
    return;
  }

  if (reg_data.volume > volume_divisor)
  {
    DEBUG_LOG_FMT(IOS_WIIMOTE, "Wiimote volume is higher than suspected maximum!");
    volume_divisor = reg_data.volume;
  }

  // SetWiimoteSpeakerVolume expects values from 0 to 255.
  // Multiply by 256, floor to int, and clamp to 255 for a uniformly mapped conversion.
  const double volume = float(reg_data.volume) * 256.f / volume_divisor;

  // This used the "Constant Power Pan Law", but it is undesirable
  // if the pan is 0, and it implied that the loudness of a wiimote speaker
  // is equal to the loudness of your device speakers, which isn't true at all.
  // This way, if the pan is 0, it's like if it is not there.
  // We should play the samples from the wiimote at the native volume they came with,
  // because you can lower their volume from the Wii settings and because they are
  // already extremely low quality, so any additional quality loss isn't welcome.
  speaker_pan = std::clamp(speaker_pan, -1.f, 1.f);
  const u32 l_volume = std::min(u32(std::min(1.f - speaker_pan, 1.f) * volume), 255u);
  const u32 r_volume = std::min(u32(std::min(1.f + speaker_pan, 1.f) * volume), 255u);

  auto& system = Core::System::GetInstance();
  SoundStream* sound_stream = system.GetSoundStream();

  sound_stream->GetMixer()->SetWiimoteSpeakerVolume(l_volume, r_volume);

  // ADPCM sample rate is thought to be x2.(3000 x2 = 6000).
  const unsigned int sample_rate = sample_rate_dividend / reg_data.sample_rate;
  sound_stream->GetMixer()->PushWiimoteSpeakerSamples(
      samples.get(), sample_length, Mixer::FIXED_SAMPLE_RATE_DIVIDEND / (sample_rate * 2));
}

void SpeakerLogic::Reset()
{
  reg_data = {};

  // Yamaha ADPCM state initialize
  adpcm_state.predictor = 0;
  adpcm_state.step = 127;
}

void SpeakerLogic::DoState(PointerWrap& p)
{
  p.Do(adpcm_state);
  p.Do(reg_data);
}

void SpeakerLogic::SetSpeakerEnabled(bool enabled)
{
  m_speaker_enabled = enabled;
}

int SpeakerLogic::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  return RawRead(&reg_data, addr, count, data_out);
}

int SpeakerLogic::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  if (0x00 == addr)
  {
    SpeakerData(data_in, count, m_speaker_pan_setting.GetValue() / 100);
    return count;
  }
  else
  {
    // TODO: Does writing immediately change the decoder config even when active
    // or does a write to 0x08 activate the new configuration or something?
    return RawWrite(&reg_data, addr, count, data_in);
  }
}

}  // namespace WiimoteEmu
