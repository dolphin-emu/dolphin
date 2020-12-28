// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <FreeSurround/FreeSurroundDecoder.h>

#include <cassert>
#include <limits>

#include "AudioCommon/Enums.h"
#include "AudioCommon/SurroundDecoder.h"
#include "Common/MathUtil.h"
#include "Core/Config/MainSettings.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "Common/Logging/Log.h" //To delete and all the uses (or make debug log)
#pragma optimize("", off) //To delete

namespace AudioCommon
{
//To move...
// Quality (higher quality means more latency).
// Returns the number of samples the DPLII decoder will take in one push (counts one channels).
// Needs to be a multiple of DECODER_GRANULARITY, or a power of it for better performance.
static u32 DPL2QualityToBlockSize(DPL2Quality quality, u32 sample_rate)
{
  u32 block_time_min, block_time_max;
  //To expose quality as int? Also try to find out what latency does physical DPLII HW actually adds
  // Ranges in ms. Make sure the average of min and max is an integer,
  // it's better if they are even as well.
  // FreeSurround said to keep it between 5 and 20ms, so this might be overkill
  // already but that's likely a mistake, the difference can be told, though on a physical decoder,
  // the added latency isn't as high?
  switch (quality)
  {
  case DPL2Quality::Low:
    // This already sounds terrible, don't go any lower
    block_time_min = 6;
    block_time_max = 20;
    block_time_min = 10; //To delete
    block_time_max = 10; //To delete
    break;
  case DPL2Quality::High:
    block_time_min = 40;
    block_time_max = 60;
    //To delete
    block_time_min = 30;
    block_time_max = 30;
    break;
  case DPL2Quality::Extreme:
    //To revert
    block_time_min = 954;
    block_time_max = 954;
    break;
  case DPL2Quality::Normal:
  default:
    block_time_min = 20;
    block_time_max = 40;
    block_time_max = 20; //To delete
  }

  double block_time_average = (block_time_min + block_time_max) * 0.5;

  u32 block_size = std::round(sample_rate * block_time_average / 1000.0);
  //block_size = MathUtil::NearestPowerOf2(block_size);
  //To delete and restore above
  block_size =
      (block_size / SurroundDecoder::DECODER_GRANULARITY) * SurroundDecoder::DECODER_GRANULARITY;

  // Find the actual used block_time to see if it's within the accepted ranges
  double block_time = block_size * 1000 / sample_rate;
  if (block_time > block_time_max || block_time < block_time_min)
  {
    block_size = std::round(sample_rate * block_time_average / 1000.0);
    block_size =
        (block_size / SurroundDecoder::DECODER_GRANULARITY) * SurroundDecoder::DECODER_GRANULARITY;
  }

  // Assert because FreeSurround wouldn't work or crash anyway, this can't be triggered as of now
  assert(block_size > 1 && block_size <= SurroundDecoder::MAX_BLOCKS_SIZE);
  return block_size;
}

//To move to AudioCommon or AudioInterface?
static inline s16 convert_float_to_s16(float sample)
{
  return s16(std::clamp(s32(sample * -std::numeric_limits<s16>::min()),
                        s32(std::numeric_limits<s16>::min()),
                        s32(std::numeric_limits<s16>::max())));
}

SurroundDecoder::SurroundDecoder(u32 sample_rate)
{
  m_fsdecoder = std::make_unique<DPL2FSDecoder>();
  Init(sample_rate);
}

SurroundDecoder::~SurroundDecoder() = default;

void SurroundDecoder::Init(u32 sample_rate)
{
  u32 block_size = DPL2QualityToBlockSize(Config::Get(Config::MAIN_DPL2_QUALITY), sample_rate);

  // Re-init. It should keep the samples in the buffer (and filling the rest with 0) while just
  // updating the settings. Variables are duplicate from the decoder because they are not exposed
  if (m_sample_rate != sample_rate || m_block_size != block_size)
  {
    m_sample_rate = sample_rate;
    m_block_size = block_size;
    m_returnable_samples_in_decoder = std::min(
        m_returnable_samples_in_decoder, m_block_size / DECODER_GRANULARITY * STEREO_CHANNELS);
    latency_warning_sent = false;
    m_fsdecoder->Init(cs_5point1, m_block_size, m_sample_rate);
  }
  //To delete
  Clear();
  // The LFE channel (bass redirection) is disabled in the surround decoder, as most people
  // have their own low pass crossover
  m_fsdecoder->set_bass_redirection(Config::Get(Config::MAIN_DPL2_BASS_REDIRECTION));
}

void SurroundDecoder::Clear()
{
  m_fsdecoder->flush();
  m_decoded_fifo.clear();
  m_float_conversion_buffer.clear();
  m_returnable_samples_in_decoder = 0;
  latency_warning_sent = false;
  memset(m_last_decoded_samples, 0.f, sizeof(m_last_decoded_samples));
}

void SurroundDecoder::PushSamples(const s16* in, u32 num_samples)
{
  if (!latency_warning_sent && num_samples > m_decoded_fifo.max_size() / SURROUND_CHANNELS)
  {
    // Increase m_decoded_fifo size if this becomes common occurrence
    latency_warning_sent = true;
    OSD::AddMessage("This DPLII Quality at this Sample Rate/Latency is not supported.\nLower your "
                    "settings to not risk missing samples",
                    6000U);
  }

  u32 read_samples = 0;
  // We do it this way so m_float_conversion_buffer never exceeds m_block_size
  // and we can be sure we won't miss samples on that front
  while (num_samples > 0)
  {
    u32 samples_to_block = 0;
    if (u32(m_float_conversion_buffer.size() / STEREO_CHANNELS) < m_block_size) //To delete condition if we resize m_float_conversion_buffer
    {
      samples_to_block = m_block_size - (u32(m_float_conversion_buffer.size() / STEREO_CHANNELS));
    }
    u32 samples_to_push = std::min(samples_to_block, num_samples);

    // Convert to float (samples are from SHRT_MIN to SHRT_MAX)
    for (size_t i = read_samples * STEREO_CHANNELS;
         i < (samples_to_push + read_samples) * STEREO_CHANNELS; ++i)
    {
      m_float_conversion_buffer.push(in[i] / float(-std::numeric_limits<s16>::min()));
    }
    read_samples += samples_to_push;
    num_samples -= samples_to_push;

    // We have enough samples to process a block
    if (m_float_conversion_buffer.size() >= m_block_size * STEREO_CHANNELS)
    {
      // Decode. Note that output isn't clamped within -1.f and +1.f.
      // Will return half silent (~0) samples for the first call
      const float* out = m_fsdecoder->decode(
          &m_float_conversion_buffer.front(), &m_float_conversion_buffer.beginning(),
          u32(m_float_conversion_buffer.head_to_end() / STEREO_CHANNELS));
      m_float_conversion_buffer_copy = std::vector<float>(m_float_conversion_buffer.size());
      m_float_conversion_buffer.copy_to_array(&m_float_conversion_buffer_copy[0],
                                              m_float_conversion_buffer.size());
      m_float_conversion_buffer.erase(m_block_size * STEREO_CHANNELS);

      //To review: quality is trash? Is there a mapping problem? Compare with vanilla dolphin (Backends: FL | FR | FC | LFE | BL | BR)
      // FreeSurround mapping has been modified to match
      m_decoded_fifo.push_array(out, m_block_size * SURROUND_CHANNELS);

      m_returnable_samples_in_decoder = m_block_size / DECODER_GRANULARITY * STEREO_CHANNELS;
    }
  }
}

//To review: sometimes when enabling DPLII in the middle of a game you hear cracking (mixed with DPLII decent output). This is because until we have stabilized the additional DPLII latency, we
//should read from m_decoded_fifo or we'd cause missed alignment. A better alternative would be to pre-fill them with 0 by calculating their number, but would it work when the backend latency suddenly changes?
void SurroundDecoder::GetDecodedSamples(float* out, u32 num_samples)
{
  if (num_samples == 0)
    return;

  static u32 max_count = 0;
  // Find the expected latency at the current setup
  u32 latency_samples = 0;
  u32 samples_in = num_samples;
  u32 block_size = m_block_size;
  u32 in_buffer = 0;
  u32 out_buffer = 0;
  u32 count = 0;
  static std::vector<float> v1;
  static std::vector<float> v2;
  static std::vector<float> v3;
  //To do: try this and make sure it doesn't stall for any combination of numbers, though it shouldn't as it's basically the LCM
  for (u32 i = 1; i < 4800; ++i)
  {
    samples_in = i;
    for (u32 k = 2; k < 2400; k += 2)
    {
      block_size = k;
      latency_samples = 0;
      in_buffer = 0;
      out_buffer = 0;
      count = 0;
      while (true)
      {
        in_buffer += samples_in;
        out_buffer += (in_buffer / block_size) * block_size;
        in_buffer = in_buffer % block_size;
        out_buffer -= std::min(out_buffer, samples_in);
        // We are done, we have found the "LCM"
        if (latency_samples == in_buffer + out_buffer)
        {
          //To find the min needed count...
          if (count >= 200)
          {
            break;
          }
          ++count;
        }
        else
        {
          if (count > 0)
          {
            v1.push_back(samples_in);
            v2.push_back(block_size);
            v3.push_back(count);
            if (count > max_count)
            {
              // INFO_LOG(AUDIO_INTERFACE, "count is broken %u", count);
              max_count = count;
            }
          }
          latency_samples = in_buffer + out_buffer;
        }
      }
    }
  }
  INFO_LOG(AUDIO_INTERFACE, "max_count %u", max_count);

  //To do: scale m_decoded_fifo to the bigger multiple of the new block size that it can contain?
  //and m_float_conversion_buffer to the min between prev num and new block size.
  //Or actually what you want is to have the new latency be half of the block size so base it on that?
  u32 buffered_samples = (u32(m_float_conversion_buffer.size()) / STEREO_CHANNELS) +
                         (u32(m_decoded_fifo.size()) / SURROUND_CHANNELS);
  static bool enable_safety = false; //To review: we don't wanna do this at the end though? We'd miss the last samples
  // We need to have built up at least a block size of samples, otherwise in the next request we might not have any left
  //To review: this actually isn't right as it doesn't consider that in the next frame we MIGHT have enough samples to continue? If so, we should append them to the right side of the out array?
  if (enable_safety && buffered_samples < m_block_size)
  {
    // No need for padding or zeroing, this can only happen at the beginning, before we have any samples
    //To review: if we increased DPLII quality, or the backend unexpectedly changed latency, or the mixer returned less samples than usual, this might happen so we do want to pad
    return;
  }

  //To review: the order of this code, shouldn't it be inverted? And all the ones in ReturnSamples()
  u32 read_samples = s32(m_decoded_fifo.copy_to_array(out, num_samples * SURROUND_CHANNELS));
  if (read_samples > 0)
  {
    m_decoded_fifo.erase(read_samples);
    memcpy(&m_last_decoded_samples[0], &out[read_samples - SURROUND_CHANNELS],
           sizeof(float) * SURROUND_CHANNELS);
  }

  //To fixup and comment out. Also, it seems like the added latency ISN'T half the block size at all... Bring back the latency alignment code??? Though explain it.
  //If backend latency is a multiple of the block size, then the extra latency is 0. If it's a dividend or a weird multiplier (like 1.5 times), latency is smaller than when it's not but it's not 0.
  //double times_to_make_a_block = double(m_block_size) / num_samples;
  //u32 min_dpli_latency = m_block_size / DECODER_GRANULARITY;
  buffered_samples = (u32(m_float_conversion_buffer.size()) / STEREO_CHANNELS) + (u32(m_decoded_fifo.size()) / SURROUND_CHANNELS);
  u32 added_dpl2_latency = buffered_samples;
  u32 guessed_added_dpl2_latency1 = (num_samples * ((MathUtil::LCM(num_samples, m_block_size) / num_samples) - 1));
  guessed_added_dpl2_latency1 = MathUtil::LCM(num_samples, m_block_size) - num_samples;
  guessed_added_dpl2_latency1 = latency_samples;
  u32 guessed_added_dpl2_latency2 = m_block_size * ((MathUtil::LCM(num_samples, m_block_size) / m_block_size) - 1);
  u32 guessed_added_dpl2_latency3 = m_block_size * ((MathUtil::LCM(num_samples, m_block_size) / num_samples) - 1); // This seems to be the best one (often correct, or a multiple/divident of the actual added latency)
  u32 guessed_added_dpl2_latency4 = num_samples * ((MathUtil::LCM(num_samples, m_block_size) / m_block_size) - 1);
  s32 lcm1 = MathUtil::LCM(num_samples, m_block_size);
  s32 lcm2 = MathUtil::LCM(num_samples - m_block_size, m_block_size);
  s32 lcm3 = MathUtil::LCM(num_samples, m_block_size - num_samples);
  s32 lcm4 = MathUtil::LCM(m_block_size - num_samples, m_block_size);
  s32 lcm5 = MathUtil::LCM(num_samples - m_block_size, num_samples);
  INFO_LOG(AUDIO_INTERFACE,
           "backend_latency %u  block_size %u  "
           "added_dpl2_latency %u  ga_dpl2_latency1 %u  ga_dpl2_latency2 "
           "%u  ga_dpl2_latency3 %u, gad_dpl2_latency4 %u  LCM1 %u  LCM2 %u  LCM3 %u  LCM4 "
           "%u  LCM5 %u",
           num_samples, m_block_size, added_dpl2_latency, guessed_added_dpl2_latency1,
           guessed_added_dpl2_latency2, guessed_added_dpl2_latency3, guessed_added_dpl2_latency4,
           lcm1, lcm2, lcm3, lcm4, lcm5);

  //To review: this breaks latency alignment when changing sample rate or quality (e.g. having a fixed larger latency in the DPLII decoder that will never go)
  // Padding should never happen. Once we have reached enough samples in the buffer,
  // it should always keep that number, unless the backend latency changes
  while (read_samples < num_samples * SURROUND_CHANNELS)
  {
    memcpy(&out[read_samples], &m_last_decoded_samples[0], sizeof(float) * SURROUND_CHANNELS);
    read_samples += SURROUND_CHANNELS;
  }
}

//To review: return order (and test...)
u32 SurroundDecoder::ReturnSamples(s16* out, u32 num_samples)
{
  u32 writable_samples = num_samples * STEREO_CHANNELS;

  // We first return (and unconvert) the samples already converted by the m_fsdecoder
  u32 readable_samples =
      std::min(u32(m_decoded_fifo.size()) / SURROUND_CHANNELS * STEREO_CHANNELS, writable_samples);
  const float center_volume = float(std::sin(MathUtil::PI / 4));  // Constant Power Pan Law
  for (size_t i = 0; i < readable_samples; ++i)
  {
    // Theoretically we should unmix the 5.1 into 2.0 better but it's really minor
    const size_t c = i % STEREO_CHANNELS;  // stereo channel
    const size_t s = (i / STEREO_CHANNELS) * SURROUND_CHANNELS;  // surround shift
    out[i] = convert_float_to_s16(m_decoded_fifo[c + s]);

    // Add back the center (index 2) to L and R and reduce its volume
    const s16 center = convert_float_to_s16(m_decoded_fifo[2 + s] * center_volume);
    out[i] = s16(std::clamp(s32(out[i] + center), s32(std::numeric_limits<s16>::min()),
                          s32(std::numeric_limits<s16>::max())));
  }
  m_decoded_fifo.erase(readable_samples / STEREO_CHANNELS * SURROUND_CHANNELS);
  writable_samples -= readable_samples;
  u32 written_samples = readable_samples;

  // Then we return the yet unconverted samples within the m_fsdecoder
  readable_samples = std::min(m_returnable_samples_in_decoder, writable_samples);
  for (size_t i = written_samples, k = (m_block_size * DECODER_GRANULARITY / STEREO_CHANNELS) -
                                       m_returnable_samples_in_decoder;
       i < readable_samples + written_samples; ++i, ++k)
  {
    out[i] = convert_float_to_s16(m_fsdecoder->input_buffer()[k]);
    m_fsdecoder->input_buffer()[k] = 0.f;
  }
  m_returnable_samples_in_decoder -= readable_samples;
  writable_samples -= readable_samples;
  written_samples += readable_samples;

  // Then the ones from the pending block push
  readable_samples = std::min(u32(m_float_conversion_buffer.size()), writable_samples);
  for (size_t i = 0; i < readable_samples; ++i)
  {
    out[i + written_samples] = convert_float_to_s16(m_float_conversion_buffer[i]);
  }
  m_float_conversion_buffer.erase(readable_samples);
  written_samples += readable_samples;

  return written_samples / STEREO_CHANNELS;
}

bool SurroundDecoder::CanReturnSamples() const
{
  return m_decoded_fifo.size() > 0 || m_returnable_samples_in_decoder > 0 ||
         m_float_conversion_buffer.size() > 0;
}
}  // namespace AudioCommon
