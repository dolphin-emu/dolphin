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
  constexpr u32 half = 0x80000000;

  const u64 out_sample_rate = m_mixer->m_output_sample_rate;
  u64 in_sample_rate = FIXED_SAMPLE_RATE_DIVIDEND / m_input_sample_rate_divisor;

  const float emulation_speed = m_mixer->m_config_emulation_speed;
  if (0 < emulation_speed && emulation_speed != 1.0)
    in_sample_rate = static_cast<u64>(std::llround(in_sample_rate * emulation_speed));

  const u32 index_jump = (in_sample_rate << GRANULE_BUFFER_FRAC_BITS) / (out_sample_rate);

  const StereoPair volume{m_LVolume.load() / 256.0f, m_RVolume.load() / 256.0f};

  while (num_samples-- > 0)
  {
    StereoPair sample = Granule::InterpStereoPair(m_front, m_back, m_current_index);
    sample *= volume;

    sample.l += samples[0] + m_quantization_error.l;
    samples[0] = ToShort(std::lround(sample.l));
    m_quantization_error.l = std::clamp(sample.l - samples[0], -1.0f, 1.0f);

    sample.r += samples[1] + m_quantization_error.r;
    samples[1] = ToShort(std::lround(sample.r));
    m_quantization_error.r = std::clamp(sample.r - samples[1], -1.0f, 1.0f);

    samples += 2;

    m_current_index += index_jump;
    if (m_current_index < half)
    {
      m_front = m_back;
      Dequeue(&m_back);

      m_current_index += half;
    }
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

    m_buffer[m_buffer_index] = StereoPair(l, r);
    m_buffer_index = (m_buffer_index + 1) & GRANULE_BUFFER_MASK;
    samples += 2;

    if (m_buffer_index == 0 || m_buffer_index == m_buffer.size() / 2)
      Enqueue(Granule(m_buffer, m_buffer_index));
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
  m_audio_fill_gaps = Config::Get(Config::MAIN_AUDIO_FILL_GAPS);
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

void Mixer::MixerFifo::Enqueue(const Granule& granule)
{
  const std::size_t head = m_queue_head.load(std::memory_order_relaxed);

  std::size_t next_head = (head + 1) % GRANULE_QUEUE_SIZE;
  if (next_head == m_queue_tail.load(std::memory_order_acquire))
    next_head = (head + GRANULE_QUEUE_SIZE / 2) % GRANULE_QUEUE_SIZE;

  m_queue[head] = granule;
  m_queue_head.store(next_head, std::memory_order_release);

  m_queue_looping.store(false, std::memory_order_relaxed);
}

void Mixer::MixerFifo::Dequeue(Granule* granule)
{
  // import numpy as np
  // import scipy.signal as signal
  // window = np.cumsum(signal.windows.dpss(32, 10))[::-1]
  // window /= window.max()
  // elements = ", ".join([f"{x:.10f}f" for x in window])
  // print(f'constexpr std::array<StereoPair, {len(window)}> FADE_WINDOW = {{ {elements} }};')
  constexpr std::array<float, 32> FADE_WINDOW = {
      1.0000000000f, 0.9999999932f, 0.9999998472f, 0.9999982765f, 0.9999870876f, 0.9999278274f,
      0.9996794215f, 0.9988227502f, 0.9963278433f, 0.9900772448f, 0.9764215513f, 0.9501402658f,
      0.9052392639f, 0.8367449916f, 0.7430540364f, 0.6277889467f, 0.5000000000f, 0.3722110533f,
      0.2569459636f, 0.1632550084f, 0.0947607361f, 0.0498597342f, 0.0235784487f, 0.0099227552f,
      0.0036721567f, 0.0011772498f, 0.0003205785f, 0.0000721726f, 0.0000129124f, 0.0000017235f,
      0.0000001528f, 0.0000000068f};

  const std::size_t tail = m_queue_tail.load(std::memory_order_relaxed);
  std::size_t next_tail = (tail + 1) % GRANULE_QUEUE_SIZE;

  if (next_tail == m_queue_head.load(std::memory_order_acquire))
  {
    // Only fill gaps when running to prevent stutter on pause.
    const bool is_running = Core::GetState(Core::System::GetInstance()) == Core::State::Running;
    if (m_mixer->m_audio_fill_gaps && is_running)
    {
      next_tail = (tail + GRANULE_QUEUE_SIZE / 2) % GRANULE_QUEUE_SIZE;
      m_queue_looping.store(true, std::memory_order_relaxed);
    }
    else
    {
      *granule = Granule();
      return;
    }
  }

  if (m_queue_looping.load(std::memory_order_relaxed))
    m_queue_fade_index = std::min(m_queue_fade_index + 1, FADE_WINDOW.size() - 1);
  else
    m_queue_fade_index = 0;

  *granule = m_queue[tail];
  *granule *= StereoPair(FADE_WINDOW[m_queue_fade_index]);

  m_queue_tail.store(next_tail, std::memory_order_release);
}

// Implementation of Granule's constructor
constexpr Mixer::MixerFifo::Granule::Granule(const GranuleBuffer& input,
                                             const std::size_t start_index)
{
  // import numpy as np
  // import scipy.signal as signal
  // window = np.convolve(np.ones(128), signal.windows.dpss(128 + 1, 4))
  // window /= (window[:len(window) // 2] + window[len(window) // 2:]).max()
  // elements = ", ".join([f"{x:.10f}f" for x in window])
  // print(f'constexpr std::array<StereoPair, GRANULE_BUFFER_SIZE> GRANULE_WINDOW = {{ {elements}
  // }};')
  constexpr std::array<float, GRANULE_BUFFER_SIZE> GRANULE_WINDOW = {
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

  const auto input_middle = input.end() - start_index;
  std::ranges::rotate_copy(input, input_middle, m_buffer.begin());

  for (std::size_t i = 0; i < m_buffer.size(); ++i)
    m_buffer[i] *= StereoPair(GRANULE_WINDOW[i]);
}

Mixer::MixerFifo::StereoPair Mixer::MixerFifo::Granule::InterpStereoPair(const Granule& prev,
                                                                         const Granule& next,
                                                                         const u32 frac)
{
  const std::size_t prev_index = frac >> Mixer::MixerFifo::GRANULE_BUFFER_FRAC_BITS;
  const std::size_t next_index = prev_index - (GRANULE_BUFFER_SIZE / 2);

  const u32 frac_t = frac & ((1 << GRANULE_BUFFER_FRAC_BITS) - 1);
  const float t1 = frac_t / static_cast<float>(1 << GRANULE_BUFFER_FRAC_BITS);
  const float t2 = t1 * t1;
  const float t3 = t2 * t1;

  // The Granules are pre-windowed, so we can just add them together
  StereoPair s0 = prev.m_buffer[(prev_index - 2) & GRANULE_BUFFER_MASK] +
                  next.m_buffer[(next_index - 2) & GRANULE_BUFFER_MASK];
  StereoPair s1 = prev.m_buffer[(prev_index - 1) & GRANULE_BUFFER_MASK] +
                  next.m_buffer[(next_index - 1) & GRANULE_BUFFER_MASK];
  StereoPair s2 = prev.m_buffer[(prev_index + 0) & GRANULE_BUFFER_MASK] +
                  next.m_buffer[(next_index + 0) & GRANULE_BUFFER_MASK];
  StereoPair s3 = prev.m_buffer[(prev_index + 1) & GRANULE_BUFFER_MASK] +
                  next.m_buffer[(next_index + 1) & GRANULE_BUFFER_MASK];
  StereoPair s4 = prev.m_buffer[(prev_index + 2) & GRANULE_BUFFER_MASK] +
                  next.m_buffer[(next_index + 2) & GRANULE_BUFFER_MASK];
  StereoPair s5 = prev.m_buffer[(prev_index + 3) & GRANULE_BUFFER_MASK] +
                  next.m_buffer[(next_index + 3) & GRANULE_BUFFER_MASK];

  s0 *= StereoPair{(+0.0f + 1.0f * t1 - 2.0f * t2 + 1.0f * t3) / 12.0f};
  s1 *= StereoPair{(+0.0f - 8.0f * t1 + 15.0f * t2 - 7.0f * t3) / 12.0f};
  s2 *= StereoPair{(+3.0f + 0.0f * t1 - 7.0f * t2 + 4.0f * t3) / 3.0f};
  s3 *= StereoPair{(+0.0f + 2.0f * t1 + 5.0f * t2 - 4.0f * t3) / 3.0f};
  s4 *= StereoPair{(+0.0f - 1.0f * t1 - 6.0f * t2 + 7.0f * t3) / 12.0f};
  s5 *= StereoPair{(+0.0f + 0.0f * t1 + 1.0f * t2 - 1.0f * t3) / 12.0f};

  return s0 + s1 + s2 + s3 + s4 + s5;
}
