// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/Mixer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "AudioCommon/Enums.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/System.h"

static u32 DPL2QualityToFrameBlockSize(AudioCommon::DPL2Quality quality)
{
  switch (quality)
  {
  case AudioCommon::DPL2Quality::Lowest:
    return 512;
  case AudioCommon::DPL2Quality::Low:
    return 1024;
  case AudioCommon::DPL2Quality::Highest:
    return 4096;
  default:
    return 2048;
  }
}

Mixer::Mixer(u32 BackendSampleRate)
    : m_output_sample_rate(BackendSampleRate),
      m_surround_decoder(BackendSampleRate,
                         DPL2QualityToFrameBlockSize(Config::Get(Config::MAIN_DPL2_QUALITY)))
{
  m_config_changed_callback_id = Config::AddConfigChangedCallback([this] { RefreshConfig(); });
  RefreshConfig();

  INFO_LOG_FMT(AUDIO_INTERFACE, "Mixer is initialized");
}

Mixer::~Mixer()
{
  Config::RemoveConfigChangedCallback(m_config_changed_callback_id);
}

void Mixer::DoState(PointerWrap& p)
{
  m_dma_mixer.DoState(p);
  m_streaming_mixer.DoState(p);
  m_wiimote_speaker_mixer.DoState(p);
  m_skylander_portal_mixer.DoState(p);
  for (auto& mixer : m_gba_mixers)
    mixer.DoState(p);
}

// Executed from sound stream thread
void Mixer::MixerFifo::Mix(s16* samples, std::size_t num_samples)
{
  constexpr u32 INDEX_HALF = 0x80000000;
  constexpr DT_s FADE_IN_RC = DT_s(0.008);
  constexpr DT_s FADE_OUT_RC = DT_s(0.064);

  const u64 out_sample_rate = m_mixer->m_output_sample_rate;
  u64 in_sample_rate = FIXED_SAMPLE_RATE_DIVIDEND / m_input_sample_rate_divisor;

  const float emulation_speed = m_mixer->m_config_emulation_speed;
  if (0 < emulation_speed && emulation_speed != 1.0)
    in_sample_rate = static_cast<u64>(std::llround(in_sample_rate * emulation_speed));

  const u32 index_jump = (in_sample_rate << GRANULE_FRAC_BITS) / (out_sample_rate);

  // These fade in / out multiplier are tuned to match a constant
  // fade speed regardless of the input or the output sample rate.
  const float fade_in_mul = -std::expm1(-DT_s(1.0) / (out_sample_rate * FADE_IN_RC));
  const float fade_out_mul = -std::expm1(-DT_s(1.0) / (out_sample_rate * FADE_OUT_RC));

  const StereoPair volume{m_LVolume.load() / 256.0f, m_RVolume.load() / 256.0f};

  // Calculate the ideal length of the granule queue.
  const std::size_t buffer_size_ms = m_mixer->m_config_audio_buffer_ms;
  const std::size_t buffer_size_samples = buffer_size_ms * in_sample_rate / 1000;

  // Limit the possible queue sizes to any number between 4 and 64.
  const std::size_t buffer_size_granules =
      std::clamp((buffer_size_samples) / (GRANULE_SIZE >> 1), static_cast<std::size_t>(4),
                 static_cast<std::size_t>(MAX_GRANULE_QUEUE_SIZE));

  m_granule_queue_size.store(buffer_size_granules, std::memory_order_relaxed);

  while (num_samples-- > 0)
  {
    // The indexes for the front and back buffers are offset by 50% of the granule size.
    // We use the modular nature of 32-bit integers to wrap around the granule size.
    m_current_index += index_jump;
    const u32 front_index = m_current_index;
    const u32 back_index = m_current_index + INDEX_HALF;

    // If either index is less than the index jump, that means we reached
    // the end of the of the buffer and need to load the next granule.
    if (front_index < index_jump)
      Dequeue(&m_front);
    else if (back_index < index_jump)
      Dequeue(&m_back);

    // The Granules are pre-windowed, so we can just add them together
    const std::size_t ft = front_index >> GRANULE_FRAC_BITS;
    const std::size_t bt = back_index >> GRANULE_FRAC_BITS;
    const StereoPair s0 = m_front[(ft - 2) & GRANULE_MASK] + m_back[(bt - 2) & GRANULE_MASK];
    const StereoPair s1 = m_front[(ft - 1) & GRANULE_MASK] + m_back[(bt - 1) & GRANULE_MASK];
    const StereoPair s2 = m_front[(ft + 0) & GRANULE_MASK] + m_back[(bt + 0) & GRANULE_MASK];
    const StereoPair s3 = m_front[(ft + 1) & GRANULE_MASK] + m_back[(bt + 1) & GRANULE_MASK];
    const StereoPair s4 = m_front[(ft + 2) & GRANULE_MASK] + m_back[(bt + 2) & GRANULE_MASK];
    const StereoPair s5 = m_front[(ft + 3) & GRANULE_MASK] + m_back[(bt + 3) & GRANULE_MASK];

    // Polynomial Interpolators for High-Quality Resampling of
    // Over Sampled Audio by Olli Niemitalo, October 2001.
    // Page 43 -- 6-point, 3rd-order Hermite:
    // https://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf
    const u32 t_frac = m_current_index & ((1 << GRANULE_FRAC_BITS) - 1);
    const float t1 = t_frac / static_cast<float>(1 << GRANULE_FRAC_BITS);
    const float t2 = t1 * t1;
    const float t3 = t2 * t1;

    StereoPair sample = (s0 * StereoPair{(+0.0f + 1.0f * t1 - 2.0f * t2 + 1.0f * t3) / 12.0f} +
                         s1 * StereoPair{(+0.0f - 8.0f * t1 + 15.0f * t2 - 7.0f * t3) / 12.0f} +
                         s2 * StereoPair{(+3.0f + 0.0f * t1 - 7.0f * t2 + 4.0f * t3) / 3.0f} +
                         s3 * StereoPair{(+0.0f + 2.0f * t1 + 5.0f * t2 - 4.0f * t3) / 3.0f} +
                         s4 * StereoPair{(+0.0f - 1.0f * t1 - 6.0f * t2 + 7.0f * t3) / 12.0f} +
                         s5 * StereoPair{(+0.0f + 0.0f * t1 + 1.0f * t2 - 1.0f * t3) / 12.0f});

    // Apply Fade In / Fade Out depending on if we are looping
    if (m_queue_looping.load(std::memory_order_relaxed))
      m_fade_volume += fade_out_mul * (0.0f - m_fade_volume);
    else
      m_fade_volume += fade_in_mul * (1.0f - m_fade_volume);

    // Apply the fade volume and the regular volume to the sample
    sample = sample * volume * StereoPair{m_fade_volume};

    // This quantization method prevents accumulated error but does not do noise shaping.
    sample.l += samples[0] - m_quantization_error.l;
    samples[0] = MathUtil::SaturatingCast<s16>(std::lround(sample.l));
    m_quantization_error.l = std::clamp(samples[0] - sample.l, -1.0f, 1.0f);

    sample.r += samples[1] - m_quantization_error.r;
    samples[1] = MathUtil::SaturatingCast<s16>(std::lround(sample.r));
    m_quantization_error.r = std::clamp(samples[1] - sample.r, -1.0f, 1.0f);

    samples += 2;
  }
}

std::size_t Mixer::Mix(s16* samples, std::size_t num_samples)
{
  if (!samples)
    return 0;

  memset(samples, 0, num_samples * 2 * sizeof(s16));

  m_dma_mixer.Mix(samples, num_samples);
  m_streaming_mixer.Mix(samples, num_samples);
  m_wiimote_speaker_mixer.Mix(samples, num_samples);
  m_skylander_portal_mixer.Mix(samples, num_samples);
  for (auto& mixer : m_gba_mixers)
    mixer.Mix(samples, num_samples);

  return num_samples;
}

std::size_t Mixer::MixSurround(float* samples, std::size_t num_samples)
{
  if (!num_samples)
    return 0;

  memset(samples, 0, num_samples * SURROUND_CHANNELS * sizeof(float));

  std::size_t needed_frames = m_surround_decoder.QueryFramesNeededForSurroundOutput(num_samples);

  constexpr std::size_t max_samples = 0x8000;
  ASSERT_MSG(AUDIO, needed_frames <= max_samples,
             "needed_frames would overflow m_scratch_buffer: {} -> {} > {}", num_samples,
             needed_frames, max_samples);

  std::array<s16, max_samples> buffer;
  std::size_t available_frames = Mix(buffer.data(), static_cast<std::size_t>(needed_frames));
  if (available_frames != needed_frames)
  {
    ERROR_LOG_FMT(AUDIO,
                  "Error decoding surround frames: needed {} frames for {} samples but got {}",
                  needed_frames, num_samples, available_frames);
    return 0;
  }

  m_surround_decoder.PutFrames(buffer.data(), needed_frames);
  m_surround_decoder.ReceiveFrames(samples, num_samples);

  return num_samples;
}

void Mixer::MixerFifo::PushSamples(const s16* samples, std::size_t num_samples)
{
  while (num_samples-- > 0)
  {
    const s16 l = m_little_endian ? samples[1] : Common::swap16(samples[1]);
    const s16 r = m_little_endian ? samples[0] : Common::swap16(samples[0]);
    samples += 2;

    m_next_buffer[m_next_buffer_index] = StereoPair(l, r);
    m_next_buffer_index = (m_next_buffer_index + 1) & GRANULE_MASK;

    // The granules overlap by 50%, so we need to enqueue the
    // next buffer every time we fill half of the samples.
    if (m_next_buffer_index == 0 || m_next_buffer_index == m_next_buffer.size() / 2)
      Enqueue();
  }
}

void Mixer::PushSamples(const s16* samples, std::size_t num_samples)
{
  m_dma_mixer.PushSamples(samples, num_samples);
  if (m_log_dsp_audio)
  {
    const s32 sample_rate_divisor = m_dma_mixer.GetInputSampleRateDivisor();
    auto volume = m_dma_mixer.GetVolume();
    m_wave_writer_dsp.AddStereoSamplesBE(samples, static_cast<u32>(num_samples),
                                         sample_rate_divisor, volume.first, volume.second);
  }
}

void Mixer::PushStreamingSamples(const s16* samples, std::size_t num_samples)
{
  m_streaming_mixer.PushSamples(samples, num_samples);
  if (m_log_dtk_audio)
  {
    const s32 sample_rate_divisor = m_streaming_mixer.GetInputSampleRateDivisor();
    auto volume = m_streaming_mixer.GetVolume();
    m_wave_writer_dtk.AddStereoSamplesBE(samples, static_cast<u32>(num_samples),
                                         sample_rate_divisor, volume.first, volume.second);
  }
}

void Mixer::PushWiimoteSpeakerSamples(const s16* samples, std::size_t num_samples,
                                      u32 sample_rate_divisor)
{
  // Max 20 bytes/speaker report, may be 4-bit ADPCM so multiply by 2
  static constexpr std::size_t MAX_SPEAKER_SAMPLES = 20 * 2;
  std::array<s16, MAX_SPEAKER_SAMPLES * 2> samples_stereo;

  ASSERT_MSG(AUDIO, num_samples <= MAX_SPEAKER_SAMPLES,
             "num_samples would overflow samples_stereo: {} > {}", num_samples,
             MAX_SPEAKER_SAMPLES);
  if (num_samples <= MAX_SPEAKER_SAMPLES)
  {
    m_wiimote_speaker_mixer.SetInputSampleRateDivisor(sample_rate_divisor);

    for (std::size_t i = 0; i < num_samples; ++i)
    {
      samples_stereo[i * 2] = samples[i];
      samples_stereo[i * 2 + 1] = samples[i];
    }

    m_wiimote_speaker_mixer.PushSamples(samples_stereo.data(), num_samples);
  }
}

void Mixer::PushSkylanderPortalSamples(const u8* samples, std::size_t num_samples)
{
  // Skylander samples are always supplied as 64 bytes, 32 x 16 bit samples
  // The portal speaker is 1 channel, so duplicate and play as stereo audio
  static constexpr std::size_t MAX_PORTAL_SPEAKER_SAMPLES = 32;
  std::array<s16, MAX_PORTAL_SPEAKER_SAMPLES * 2> samples_stereo;

  ASSERT_MSG(AUDIO, num_samples <= MAX_PORTAL_SPEAKER_SAMPLES,
             "num_samples is not less or equal to 32: {} > {}", num_samples,
             MAX_PORTAL_SPEAKER_SAMPLES);

  if (num_samples <= MAX_PORTAL_SPEAKER_SAMPLES)
  {
    for (std::size_t i = 0; i < num_samples; ++i)
    {
      s16 sample = static_cast<u16>(samples[i * 2 + 1]) << 8 | static_cast<u16>(samples[i * 2]);
      samples_stereo[i * 2] = sample;
      samples_stereo[i * 2 + 1] = sample;
    }

    m_skylander_portal_mixer.PushSamples(samples_stereo.data(), num_samples);
  }
}

void Mixer::PushGBASamples(std::size_t device_number, const s16* samples, std::size_t num_samples)
{
  m_gba_mixers[device_number].PushSamples(samples, num_samples);
}

void Mixer::SetDMAInputSampleRateDivisor(u32 rate_divisor)
{
  m_dma_mixer.SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetStreamInputSampleRateDivisor(u32 rate_divisor)
{
  m_streaming_mixer.SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetGBAInputSampleRateDivisors(std::size_t device_number, u32 rate_divisor)
{
  m_gba_mixers[device_number].SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetStreamingVolume(u32 lvolume, u32 rvolume)
{
  m_streaming_mixer.SetVolume(std::clamp<u32>(lvolume, 0x00, 0xff),
                              std::clamp<u32>(rvolume, 0x00, 0xff));
}

void Mixer::SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume)
{
  m_wiimote_speaker_mixer.SetVolume(lvolume, rvolume);
}

void Mixer::SetGBAVolume(std::size_t device_number, u32 lvolume, u32 rvolume)
{
  m_gba_mixers[device_number].SetVolume(lvolume, rvolume);
}

void Mixer::StartLogDTKAudio(const std::string& filename)
{
  if (!m_log_dtk_audio)
  {
    bool success = m_wave_writer_dtk.Start(filename, m_streaming_mixer.GetInputSampleRateDivisor());
    if (success)
    {
      m_log_dtk_audio = true;
      m_wave_writer_dtk.SetSkipSilence(false);
      NOTICE_LOG_FMT(AUDIO, "Starting DTK Audio logging");
    }
    else
    {
      m_wave_writer_dtk.Stop();
      NOTICE_LOG_FMT(AUDIO, "Unable to start DTK Audio logging");
    }
  }
  else
  {
    WARN_LOG_FMT(AUDIO, "DTK Audio logging has already been started");
  }
}

void Mixer::StopLogDTKAudio()
{
  if (m_log_dtk_audio)
  {
    m_log_dtk_audio = false;
    m_wave_writer_dtk.Stop();
    NOTICE_LOG_FMT(AUDIO, "Stopping DTK Audio logging");
  }
  else
  {
    WARN_LOG_FMT(AUDIO, "DTK Audio logging has already been stopped");
  }
}

void Mixer::StartLogDSPAudio(const std::string& filename)
{
  if (!m_log_dsp_audio)
  {
    bool success = m_wave_writer_dsp.Start(filename, m_dma_mixer.GetInputSampleRateDivisor());
    if (success)
    {
      m_log_dsp_audio = true;
      m_wave_writer_dsp.SetSkipSilence(false);
      NOTICE_LOG_FMT(AUDIO, "Starting DSP Audio logging");
    }
    else
    {
      m_wave_writer_dsp.Stop();
      NOTICE_LOG_FMT(AUDIO, "Unable to start DSP Audio logging");
    }
  }
  else
  {
    WARN_LOG_FMT(AUDIO, "DSP Audio logging has already been started");
  }
}

void Mixer::StopLogDSPAudio()
{
  if (m_log_dsp_audio)
  {
    m_log_dsp_audio = false;
    m_wave_writer_dsp.Stop();
    NOTICE_LOG_FMT(AUDIO, "Stopping DSP Audio logging");
  }
  else
  {
    WARN_LOG_FMT(AUDIO, "DSP Audio logging has already been stopped");
  }
}

void Mixer::RefreshConfig()
{
  m_config_emulation_speed = Config::Get(Config::MAIN_EMULATION_SPEED);
  m_config_fill_audio_gaps = Config::Get(Config::MAIN_AUDIO_FILL_GAPS);
  m_config_audio_buffer_ms = Config::Get(Config::MAIN_AUDIO_BUFFER_SIZE);
}

void Mixer::MixerFifo::DoState(PointerWrap& p)
{
  p.Do(m_input_sample_rate_divisor);
  p.Do(m_LVolume);
  p.Do(m_RVolume);
}

void Mixer::MixerFifo::SetInputSampleRateDivisor(u32 rate_divisor)
{
  m_input_sample_rate_divisor = rate_divisor;
}

u32 Mixer::MixerFifo::GetInputSampleRateDivisor() const
{
  return m_input_sample_rate_divisor;
}

void Mixer::MixerFifo::SetVolume(u32 lvolume, u32 rvolume)
{
  m_LVolume.store(lvolume + (lvolume >> 7));
  m_RVolume.store(rvolume + (rvolume >> 7));
}

std::pair<s32, s32> Mixer::MixerFifo::GetVolume() const
{
  return std::make_pair(m_LVolume.load(), m_RVolume.load());
}

void Mixer::MixerFifo::Enqueue()
{
  // import numpy as np
  // import scipy.signal as signal
  // window = np.convolve(np.ones(128), signal.windows.dpss(128 + 1, 4))
  // window /= (window[:len(window) // 2] + window[len(window) // 2:]).max()
  // elements = ", ".join([f"{x:.10f}f" for x in window])
  // print(f'constexpr std::array<StereoPair, GRANULE_SIZE> GRANULE_WINDOW = {{ {elements}
  // }};')
  constexpr std::array<StereoPair, GRANULE_SIZE> GRANULE_WINDOW = {
      0.0000016272f, 0.0000050749f, 0.0000113187f, 0.0000216492f, 0.0000377350f, 0.0000616906f,
      0.0000961509f, 0.0001443499f, 0.0002102045f, 0.0002984010f, 0.0004144844f, 0.0005649486f,
      0.0007573262f, 0.0010002765f, 0.0013036694f, 0.0016786636f, 0.0021377783f, 0.0026949534f,
      0.0033656000f, 0.0041666352f, 0.0051165029f, 0.0062351752f, 0.0075441359f, 0.0090663409f,
      0.0108261579f, 0.0128492811f, 0.0151626215f, 0.0177941726f, 0.0207728499f, 0.0241283062f,
      0.0278907219f, 0.0320905724f, 0.0367583739f, 0.0419244083f, 0.0476184323f, 0.0538693708f,
      0.0607049996f, 0.0681516192f, 0.0762337261f, 0.0849736833f, 0.0943913952f, 0.1045039915f,
      0.1153255250f, 0.1268666867f, 0.1391345431f, 0.1521323012f, 0.1658591025f, 0.1803098534f,
      0.1954750915f, 0.2113408944f, 0.2278888303f, 0.2450959552f, 0.2629348550f, 0.2813737361f,
      0.3003765625f, 0.3199032396f, 0.3399098438f, 0.3603488941f, 0.3811696664f, 0.4023185434f,
      0.4237393998f, 0.4453740162f, 0.4671625177f, 0.4890438330f, 0.5109561670f, 0.5328374823f,
      0.5546259838f, 0.5762606002f, 0.5976814566f, 0.6188303336f, 0.6396511059f, 0.6600901562f,
      0.6800967604f, 0.6996234375f, 0.7186262639f, 0.7370651450f, 0.7549040448f, 0.7721111697f,
      0.7886591056f, 0.8045249085f, 0.8196901466f, 0.8341408975f, 0.8478676988f, 0.8608654569f,
      0.8731333133f, 0.8846744750f, 0.8954960085f, 0.9056086048f, 0.9150263167f, 0.9237662739f,
      0.9318483808f, 0.9392950004f, 0.9461306292f, 0.9523815677f, 0.9580755917f, 0.9632416261f,
      0.9679094276f, 0.9721092781f, 0.9758716938f, 0.9792271501f, 0.9822058274f, 0.9848373785f,
      0.9871507189f, 0.9891738421f, 0.9909336591f, 0.9924558641f, 0.9937648248f, 0.9948834971f,
      0.9958333648f, 0.9966344000f, 0.9973050466f, 0.9978622217f, 0.9983213364f, 0.9986963306f,
      0.9989997235f, 0.9992426738f, 0.9994350514f, 0.9995855156f, 0.9997015990f, 0.9997897955f,
      0.9998556501f, 0.9999038491f, 0.9999383094f, 0.9999622650f, 0.9999783508f, 0.9999886813f,
      0.9999949251f, 0.9999983728f, 0.9999983728f, 0.9999949251f, 0.9999886813f, 0.9999783508f,
      0.9999622650f, 0.9999383094f, 0.9999038491f, 0.9998556501f, 0.9997897955f, 0.9997015990f,
      0.9995855156f, 0.9994350514f, 0.9992426738f, 0.9989997235f, 0.9986963306f, 0.9983213364f,
      0.9978622217f, 0.9973050466f, 0.9966344000f, 0.9958333648f, 0.9948834971f, 0.9937648248f,
      0.9924558641f, 0.9909336591f, 0.9891738421f, 0.9871507189f, 0.9848373785f, 0.9822058274f,
      0.9792271501f, 0.9758716938f, 0.9721092781f, 0.9679094276f, 0.9632416261f, 0.9580755917f,
      0.9523815677f, 0.9461306292f, 0.9392950004f, 0.9318483808f, 0.9237662739f, 0.9150263167f,
      0.9056086048f, 0.8954960085f, 0.8846744750f, 0.8731333133f, 0.8608654569f, 0.8478676988f,
      0.8341408975f, 0.8196901466f, 0.8045249085f, 0.7886591056f, 0.7721111697f, 0.7549040448f,
      0.7370651450f, 0.7186262639f, 0.6996234375f, 0.6800967604f, 0.6600901562f, 0.6396511059f,
      0.6188303336f, 0.5976814566f, 0.5762606002f, 0.5546259838f, 0.5328374823f, 0.5109561670f,
      0.4890438330f, 0.4671625177f, 0.4453740162f, 0.4237393998f, 0.4023185434f, 0.3811696664f,
      0.3603488941f, 0.3399098438f, 0.3199032396f, 0.3003765625f, 0.2813737361f, 0.2629348550f,
      0.2450959552f, 0.2278888303f, 0.2113408944f, 0.1954750915f, 0.1803098534f, 0.1658591025f,
      0.1521323012f, 0.1391345431f, 0.1268666867f, 0.1153255250f, 0.1045039915f, 0.0943913952f,
      0.0849736833f, 0.0762337261f, 0.0681516192f, 0.0607049996f, 0.0538693708f, 0.0476184323f,
      0.0419244083f, 0.0367583739f, 0.0320905724f, 0.0278907219f, 0.0241283062f, 0.0207728499f,
      0.0177941726f, 0.0151626215f, 0.0128492811f, 0.0108261579f, 0.0090663409f, 0.0075441359f,
      0.0062351752f, 0.0051165029f, 0.0041666352f, 0.0033656000f, 0.0026949534f, 0.0021377783f,
      0.0016786636f, 0.0013036694f, 0.0010002765f, 0.0007573262f, 0.0005649486f, 0.0004144844f,
      0.0002984010f, 0.0002102045f, 0.0001443499f, 0.0000961509f, 0.0000616906f, 0.0000377350f,
      0.0000216492f, 0.0000113187f, 0.0000050749f, 0.0000016272f};

  const std::size_t head = m_queue_head.load(std::memory_order_acquire);

  // Check if we run out of space in the circular queue. (rare)
  std::size_t next_head = (head + 1) & GRANULE_QUEUE_MASK;
  if (next_head == m_queue_tail.load(std::memory_order_acquire))
  {
    WARN_LOG_FMT(AUDIO,
                 "Granule Queue has completely filled and audio samples are being dropped. "
                 "This should not happen unless the audio backend has stopped requesting audio.");
    return;
  }

  // By preconstructing the granule window, we have the best chance of
  // the compiler optimizing this loop using SIMD instructions.
  const std::size_t start_index = m_next_buffer_index;
  for (std::size_t i = 0; i < GRANULE_SIZE; ++i)
    m_queue[head][i] = m_next_buffer[(i + start_index) & GRANULE_MASK] * GRANULE_WINDOW[i];

  m_queue_head.store(next_head, std::memory_order_release);
  m_queue_looping.store(false, std::memory_order_relaxed);
}

void Mixer::MixerFifo::Dequeue(Granule* granule)
{
  const std::size_t granule_queue_size = m_granule_queue_size.load(std::memory_order_relaxed);
  const std::size_t head = m_queue_head.load(std::memory_order_acquire);
  std::size_t tail = m_queue_tail.load(std::memory_order_acquire);

  // Checks to see if the queue has gotten too long.
  if (granule_queue_size < ((head - tail) & GRANULE_QUEUE_MASK))
  {
    // Jump the playhead to half the queue size behind the head.
    const std::size_t gap = (granule_queue_size >> 1) + 1;
    tail = (head - gap) & GRANULE_QUEUE_MASK;
  }

  // Checks to see if the queue is empty.
  std::size_t next_tail = (tail + 1) & GRANULE_QUEUE_MASK;
  if (next_tail == head)
  {
    // Only fill gaps when running to prevent stutter on pause.
    const bool is_running = Core::GetState(Core::System::GetInstance()) == Core::State::Running;
    if (m_mixer->m_config_fill_audio_gaps && is_running)
    {
      // Jump the playhead to half the queue size behind the head.
      // This provides smoother audio playback than suddenly stopping.
      const std::size_t gap = std::max<std::size_t>(2, granule_queue_size >> 1) - 1;
      next_tail = (head - gap) & GRANULE_QUEUE_MASK;
      m_queue_looping.store(true, std::memory_order_relaxed);
    }
    else
    {
      std::fill(granule->begin(), granule->end(), StereoPair{0.0f, 0.0f});
      m_queue_looping.store(false, std::memory_order_relaxed);
      return;
    }
  }

  *granule = m_queue[tail];
  m_queue_tail.store(next_tail, std::memory_order_release);
}
