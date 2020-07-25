// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/Mixer.h" //To try to delete some includes and see if it builds

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include <cstdlib>
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

#include "Common/Logging/Log.h" //To delete and all the uses (or make debug log)
#include "VideoCommon/OnScreenDisplay.h" //To delete and all the uses
//#pragma optimize("", off) //To delete

Mixer::Mixer(u32 sample_rate)
    : m_sample_rate(sample_rate), m_stretcher(sample_rate),
      m_surround_decoder(sample_rate, Config::Get(Config::MAIN_DPL2_QUALITY))
{
  m_scratch_buffer.reserve(MAX_SAMPLES * NC);
  m_dma_speed.Start(true);

  INFO_LOG(AUDIO, "Mixer is initialized");

  m_on_state_changed_handle = Core::AddOnStateChangedCallback([this](Core::State state) {
    if (state == Core::State::Paused)
      SetPaused(true);
    else if (state == Core::State::Running)
      SetPaused(false);
  });
}

Mixer::~Mixer()
{
  INFO_LOG_FMT(AUDIO_INTERFACE, "Mixer is initialized");
  Core::RemoveOnStateChangedCallback(m_on_state_changed_handle);
}

void Mixer::SetPaused(bool paused)
{
  if (!m_dma_speed.IsPaused() && !paused)
  {
    // This happens on game start. The backend has already started reading actually
    m_dma_speed.Start(true);
  }
  m_dma_speed.SetPaused(paused);
}

void Mixer::DoState(PointerWrap& p)
{
  m_dma_mixer.DoState(p);
  m_streaming_mixer.DoState(p);
  m_wiimote_speaker_mixer[0].DoState(p);
  m_wiimote_speaker_mixer[1].DoState(p);
  m_wiimote_speaker_mixer[2].DoState(p);
  m_wiimote_speaker_mixer[3].DoState(p);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    m_dma_speed.SetTicksPerSecond(m_dma_mixer.GetInputSampleRate());
    // We could reset a few things here but it would require too much thread synchronization
  }
}

void Mixer::SetSampleRate(u32 sample_rate)
{
  m_sample_rate = sample_rate;
  m_stretcher.SetSampleRate(m_sample_rate);
  m_surround_decoder.SetSampleRate(m_sample_rate);
  //To review: is this thread safe between the game and emu thread?
  if (!m_dma_speed.IsPaused())
    m_dma_speed.Start(true);
  //To do: reset m_fract, and also DPLII. Add method to reset DPLII
}

// render num_samples sample pairs to samples[]
// advance indexR with sample position
// returns the new number of samples mixed (not the ones played backwards)
u32 Mixer::MixerFifo::Mix(s16* samples, u32 num_samples, bool stretching)
{
  // Cache access in non-volatile variable
  // This is the only function changing the read value, so it's safe to
  // cache it locally although it's written here.
  // The writing pointer will be modified outside, but it will only increase,
  // so we will just ignore new written data while interpolating.
  // Without this cache, the compiler wouldn't be allowed to optimize the
  // interpolation loop.
  u32 indexR = m_indexR.load();
  u32 indexW = m_indexW.load();

  // The rate can be any, unfortunately we don't apply an anti aliasing filer, which means
  // we might get aliasing at higher rates (unless our mixer sample rate is very high)
  double rate = (m_input_sample_rate * (stretching ? 1.0 : m_mixer->GetCurrentSpeed())) /
                m_mixer->m_sample_rate;

  s32 lVolume = m_lVolume.load();
  s32 rVolume = m_rVolume.load();

  s16 s[NC]; // Padding samples
  s[0] = m_last_output_samples[1];
  s[1] = m_last_output_samples[1];

  // Actual number of samples written (not padded nor backwards played)
  u32 actual_samples_count =
      CubicInterpolation(samples, num_samples, rate, indexR, indexW, s[0], s[1], lVolume, rVolume);
  m_last_output_samples[0] = s[0];
  m_last_output_samples[1] = s[1];
  if (actual_samples_count != num_samples)
  {
    if (actual_samples_count > 0)
    {
      // We reserved some samples for the interpolation so start from the opposite direction
      // to make the first backwards interpolated sample be as close as possible to the
      // second last forward played one (indexR will be increased again before reading)
      m_backwards_indexR = indexR + INTERP_SAMPLES * NC;
      m_backwards_fract = 1.0 - m_fract;
    }
    // We have run out of samples, so set indexR to the max allowed so that there won't be any
    // further attempts at reading it, when indexW is finally increased we will resume reading,
    // acknowledging that indexR had already been increased for the next time, as fract would be
    // set to -1. We could also have ignored changing it here, and set a variable like:
    // m_finished_samples, but that would have very little benefits (except more fract consistency)
    // to the cost of additional code branches. Setting indesR to indexW also makes them
    // stay more aligned (not really necessary) setting fract to -1 so it resets itself helps
    // perfect ratios to maintain quality after a small drop in speed which would have ruined fract
    // being always 0.
    // Try to pull back indexR to place it so that the next sample that will be interpolated
    // will be just one sample "behind" the last one played forwards
    indexR = indexW - INTERP_SAMPLES * NC;
    m_fract = -1.0;
  }

  // Padding (mix the remaining samples with the our last sample in case we didn't have enough)
  // indexR is always "INTERP_SAMPLES" away from indexW,
  // so pad by the current indexR, which was the biggest influence
  // against the cubic interpolation of this frame

  s32 behind_samples = num_samples - actual_samples_count;
  // This might sound bad if we are constantly missing a few samples, but that should never happen,
  // and we couldn't predict it anyway (meaning we should start playing backwards as soon as we can)
  if (behind_samples > 0 && m_constantly_pushed && !stretching)
  {
    rate = m_input_sample_rate / m_mixer->m_sample_rate; //To review (this should actually follow the rate but with no prediction...)
    s16* back_samples = samples + actual_samples_count * NC;
    // I've been thinking of this a lot and this is the bast way to deal with it.
    // If we don't have enough samples to mix the number of samples requested, we play back all samples starting from the last mixed one
    // (until we get new samples or until we still have old samples to read).
    // There are other similar ways to deal with this:
    // -Only play backwards if the number of missing samples is greater than "n", otherwise do padding.
    //    That's wrong because n is arbitrary and the required length would need to depend on the current sound wave period
    // -Play backwards half of the missing required samples, then play back forwards the other half.
    //    The good thing of this approach is that it blends well when it toggles the direction and normal playback resumes,
    //    but unfortunately, it's arbitrary as if we had a backend latency of "0", we'd mix 1 sample per time, so
    //    switching between playing old samples back and forward wouldn't be possible, unless we abstracted latency (the number of required out samples)
    //    from the code and cached a series of variables (e.g. play back 15ms of sound, then play these forwards again).
    //    This might work out better but is long to implement, but that 15ms of sound would again be an arbitrary number which might sound better or worse
    //    depending on the current sound waves
    // Once we have run out of old samples to play, this will output silence. Alternatively, we could have played them forwards again, but the likelihood of running out of samples with the
    // current buffer size is extremely rare: not worth considering.
    // ---------------------
    // I don't think constantly changing direction between backward and forward when we are out
    // of samples can ever work. I've been trying. the 2 problems are:
    // -if it's not latency agnostic, you'd play back half of the missing samples backwards, and
    // then from the point you reached, play the same set forwards again (so that if emulation
    // resumes, the last played sample would be the matching the first new sample) so the above
    // would be fine, except the way it sounds depends on your current backend latency, and if you
    // have a very low backend latency, you'd never have enough time to let a few sound waves play
    // smoothly without inverting the playback direction again
    // -if it is latency agnostic, then we'd
    // play a pre-decided number of samples backwards, and then the same number forwards again, over
    // one or more audio frames. This would keep ping-ponging until we don't have any new samples
    // again, this would be all right in some case, but then, when you actually receive the new
    // samples from the DMA, you are going to have to interrupt the ping point in a random place,
    // and that defeats the purpose of doing the ping-pong in the first place unless you wait for
    // the ping-pong cycle to be finished, but that would introduce latency which is insane if you
    // are already not at full speed the only remaining solution i can think of is to cross fade but
    // from my tests it doesn't seem to be necessary, cracklings aren't bad enough, they aren't
    // really disturbing, they are very low, and it's not like dolphin didn't have them before.
    // Also in general, ping ponging wouldn't give you enough time to appreciate the sounds,
    // they would constantly be interrupted. The only solution to crackling would be cross fade?
    CubicInterpolation(back_samples, behind_samples, rate, m_backwards_indexR, indexW, s[0], s[1],
                       lVolume, rVolume, false);
  }
  // If we are not constantly pushed, we always append some silent samples at the end of our buffer,
  // so there won't be any need of panning until new samples are pushed
  else if (behind_samples > 0 && (m_constantly_pushed || m_currently_pushed))
  {
    if (indexW > 8) OSD::AddMessage("Behind samples: " + std::to_string(behind_samples), 0U);

    //To review: should padding play the last played sample or try to predict what the next one would have been? We will resume from the previous indexR as we have lost m_fract here
    //To review: if we re-enable padding on wii mote forever, make sure it's disabled once we disconnect it
    unsigned int current_sample = actual_samples_count * 2;
    for (; current_sample < num_samples * 2; current_sample += 2)
    {
      samples[current_sample + 0] =
          std::clamp(m_last_output_samples[0] + samples[current_sample + 0], -SHRT_MAX, SHRT_MAX);
      samples[current_sample + 1] =
          std::clamp(m_last_output_samples[1] + samples[current_sample + 1], -SHRT_MAX, SHRT_MAX);
    }
  }
  //if (indexW > 8 && !m_constantly_pushed) OSD::AddMessage("last sample: " + std::to_string(m_last_output_samples[0]), 0U);

  m_indexR.store(indexR);

  return actual_samples_count;
}

// Sounds better than linear interpolation
u32 Mixer::MixerFifo::CubicInterpolation(s16* samples, u32 num_samples, double rate, u32& indexR,
                                         u32 indexW, s16& l_s, s16& r_s, s32 lVolume, s32 rVolume,
                                         bool forwards)
{
  // Coefficients copied from SoundTouch Copyright © Olli Parviainen 2001-2019
  constexpr float coeffs[] =
  { -0.5f,  1.0f, -0.5f, 0.0f,
     1.5f, -2.5f,  0.0f, 1.0f,
    -1.5f,  2.0f,  0.5f, 0.0f,
     0.5f, -0.5f,  0.0f, 0.0f };

  u32 available_samples = SamplesDifference(indexW, indexR);
  s8 direction = forwards ? 1 : -1;
  double& fract = forwards ? m_fract : m_backwards_fract;
  s16* interpolation_buffer = m_mixer->m_interpolation_buffer.data();

  u32 requested_samples = u32(rate * num_samples) * NC + NC;  // Increase by 1 for imprecisions
  u32 readable_samples = forwards ? available_samples : (MAX_SAMPLES * NC);
  u32 samples_to_read = std::min(requested_samples + (INTERP_SAMPLES * NC), readable_samples);
  u32 first_indexR = GetNextIndexR(indexR);
  u32 last_indexR = first_indexR + samples_to_read * direction;
  // Do the swaps once instead of processing them for each iteration below
  for (u32 k = first_indexR; k != last_indexR + direction * NC; k += direction * NC)
  {
    interpolation_buffer[(k + 0) & INDEX_MASK] = Common::swap16(m_buffer[(k + 0) & INDEX_MASK]);
    interpolation_buffer[(k + 1) & INDEX_MASK] = Common::swap16(m_buffer[(k + 1) & INDEX_MASK]);
  }

  // fract requested to be reset so sure it will be 0 in the first cycle
  if (fract < 0.0 && num_samples > 0 && (!forwards || available_samples > INTERP_SAMPLES * NC))
  {
    fract = -rate;
  }

  u32 i = 0;
  u32 next_available_samples = available_samples;
  // Stop 3 (INTERP_SAMPLES) samples from the end as we need to interpolate with them.
  while (i < num_samples && (!forwards || (next_available_samples > INTERP_SAMPLES * NC &&
                                          next_available_samples <= available_samples)))
  {
    // If rate is 1 it would like there was no interpolation.
    // If rate is 0 fract won't never make a whole so it's basically like padding.
    // If rate is a recurring decimal fract often ends up being 0.99999 due to
    // loss of precision, but that is absolutely fine, it implies no quality loss.
    // Fract imprecisions are the reason we don't pre-calculate the number of iterations
    fract += rate;          // Update fraction position
    u32 whole = u32(fract); // Update whole position
    fract -= whole;
    // Increase indexR before reading it, not after like before. The old code had 3 problems:
    // - IndexR was increased after being read, so if the rate was very high,
    //   it could go over indexW. This would require a flag to be fixed
    // - In our first iteration, we would use the last fract calculated from the previous
    //   interpolation (last audio frame), meaning that it would have been based on the old
    //   rate, not the new one. Of course, over time the errors would cancel themselves out,
    //   but in the immediate, it was wrong (less reactive to changes in speed)
    // - When suddenly changing playback direction, the indexR would have been moved to the
    //   next position to read in the opposite direction
    // The only "problems" with the new code is that in the very first iteration after a fract
    // reset, fract won't be increased. It also denaturalizes the progress of indexR if we
    // run out of samples, but that would sound bad anyway. We also can't really tell
    // how many samples we will have available until we have found out our next play rate
    //To review last condition above, how often does it happen? How bad it is? How could we improve on that? The wii mote speaker might be more sensible to that
    indexR += NC * whole * direction;

    available_samples = next_available_samples;
    next_available_samples =
        SamplesDifference(indexW, indexR);

    const float x2 = float(fract);  // x
    const float x1 = x2 * x2;       // x^2
    const float x0 = x1 * x2;       // x^3

    float y0 = coeffs[0]  * x0 + coeffs[1]  * x1 + coeffs[2]  * x2 + coeffs[3];
    float y1 = coeffs[4]  * x0 + coeffs[5]  * x1 + coeffs[6]  * x2 + coeffs[7];
    float y2 = coeffs[8]  * x0 + coeffs[9]  * x1 + coeffs[10] * x2 + coeffs[11];
    float y3 = coeffs[12] * x0 + coeffs[13] * x1 + coeffs[14] * x2 + coeffs[15];

    // The first and last sample act as control points, the middle ones have more importance.
    // The very first and last samples might never directly be used but it shouldn't be a problem.
    // Theoretically we could linearly interpolate between the last 2 samples
    // and trade latency with a small hit in quality, but it is not worth it.
    // We could have ignored the direction while playing backwards, and just read
    // indexR over our actually play direction, but I thought that was "wrong"
    float l_s_f = y0 * interpolation_buffer[indexR & INDEX_MASK] +
                  y1 * interpolation_buffer[(indexR + 2 * direction) & INDEX_MASK] +
                  y2 * interpolation_buffer[(indexR + 4 * direction) & INDEX_MASK] +
                  y3 * interpolation_buffer[(indexR + 6 * direction) & INDEX_MASK];
    float r_s_f = y0 * interpolation_buffer[(indexR + 1) & INDEX_MASK] +
                  y1 * interpolation_buffer[(indexR + 2 * direction + 1) & INDEX_MASK] +
                  y2 * interpolation_buffer[(indexR + 4 * direction + 1) & INDEX_MASK] +
                  y3 * interpolation_buffer[(indexR + 6 * direction + 1) & INDEX_MASK];
    //To test left and right on HW source

    l_s = (s16(std::round(l_s_f)) * lVolume) >> 8;
    r_s = (s16(std::round(r_s_f)) * rVolume) >> 8;
    // Clamp after adding to current sample, as if the cubic interpolation produced a sample over
    // the limits and the current sample has the opposite sign, then we'd keep the excess value,
    // while if it had the same sign, it would have been clamped anyway.
    // We don't clamp to SHRT_MIN to keep the center at 0, though audio devices can take SHRT_MIN
    samples[i * NC + 0] = std::clamp(samples[i * NC + 0] + l_s, -SHRT_MAX, SHRT_MAX);
    samples[i * NC + 1] = std::clamp(samples[i * NC + 1] + r_s, -SHRT_MAX, SHRT_MAX);

    ++i;
  }

  return i;
}

u32 Mixer::Mix(s16* samples, u32 num_samples)
{
  // We can't mix if the emulation is paused as m_dma_speed would return wrong speeds
  if (!samples || num_samples == 0 || m_dma_speed.IsPaused())
    return 0;

  bool stretching = SConfig::GetInstance().m_audio_stretch;

  double emulation_speed = SConfig::GetInstance().m_EmulationSpeed;
  bool frame_limiter = emulation_speed > 0.0 && !Core::GetIsThrottlerTempDisabled();

  // backend latency in seconds
  double time_delta = double(num_samples) / m_sample_rate;

  double average_actual_speed = m_dma_speed.GetCachedAverageSpeed(false, true, true);
  bool predicting = true;
  // Set predicting to false if we are not predicting (meaning the last samples push isn't late)
  double actual_speed = m_dma_speed.GetLastSpeed(predicting, true);
  //INFO_LOG(AUDIO, "dma_mixer current speed: %lf", average_actual_speed);

  double stretching_target_speed = 1.0;
  //To review: keep the previous target speed while we calculate the new one (to get the correct indexR)? Have it double buffered. Also, maintain in when pass in between stretching and not
  double& target_speed = stretching ? stretching_target_speed : m_target_speed;
  target_speed = emulation_speed;
  if (stretching)
  {
    m_target_speed = 1.0;
  }

  // What we likely want to do is check the emulation speed difference between the target speed and the averaged emulation speed,
  // but then use the last frame emulation speed for more accuracy (and clamping it to the target speed)
  static double FallbackDelta = 0.0; //0.005 or before 0.1
  // Only do if we are actually going slower, if we are going faster, it's likely to be a slight incorrection of the actual speed (because it's averaged).
  // Given that we ignore the frames where we went faster, overall we will consume less audio samples that what we have received, causing less padding.
  // This avoids missing (or padded) samples when we can't reach full speed. The delta is relative to the emulator target speed.
  // OLD: (emulation_speed - average_actual_speed > FallbackDelta)
  // Unfortunately this doesn't work when unpausing (or starting?) the emulation, so we'll need to
  // work around that.
  // When using the last 3 frames (?) averaged speed, this causes heavier crackling (and fluctuation), but overall the
  // sound is much more accurate.
  // When using the very last frame, the sound works much better and has less crackling and skips, at the cost of fluctuating in pitch a bit,
  // just like the emulation itself is doing.
  // Using the amount of samples in the buffer to calculate the emulation speed doesn't seem to be accurate at all because of so many timing/order fluctuations
  // NonStretchCorrectionTolerance: we don't want to suddenly snap in and out of actual FPS speed, but only when we have confirmed the slow down is not due to a hiccup
  // Small variances are already handled by the mixing, so we don't want to take care of these cases

  if (!frame_limiter)
  {
    m_time_behind_target_speed = 0.0; //To maybe find a way to maintain this during unlimited frame rate?
    m_time_below_target_speed_growing = false;
    //To review: this can start over 30 in the first frames of game...
    target_speed = m_dma_speed.GetCachedAverageSpeed(true, true, true);
    m_time_at_custom_speed = m_time_at_custom_speed + time_delta;
  }
  //To avoid divisions all over, also, just do these things at the top every frame...
  // actual_speed varies by about 0.5% every audio frame so we need to go back and forth and then when we have lost enough,
  // we will start using the average actual speed. To come back at full emulation speed, we check for how long we had been
  // running at full speed, we don't wait for m_time_behind_target_speed to come back to 0 because that will never happen,
  // what's lost is lost (though recovery could still happen if for some reason we had cycles imprecisions within a frame???)
  // Stop using emulation speed, start using actual speed for audio playback, if we fell behind enough
  // Filter out small inaccuracies (samples aren't submitted with perfect timing)
  else if (actual_speed / emulation_speed < 1.0 - FallbackDelta)
  {
    // Note: when not able to reach the target speed before m_time_below_target_speed_growing triggers, you might hear crackles due to (probably) having finished the samples
    //To review: this happens more often at higher backend latencies, smooth it over time? Ignore first missed frame?
    // If we fell behind of m_time_behind_target_speed seconds of samples, start using the actual emulation speed
    m_time_behind_target_speed += time_delta * (1.0 - (actual_speed / emulation_speed));
    if (SConfig::GetInstance().m_audio_emu_speed_tolerance >= 0 &&
        m_time_behind_target_speed > SConfig::GetInstance().m_audio_emu_speed_tolerance / 1000.0)
    {
      if (!m_time_below_target_speed_growing)
        OSD::AddMessage("m_time_below_target_speed_growing = true", 2000U);
      //m_time_behind_target_speed = SConfig::GetInstance().NonStretchCorrectionTolerance;
      m_time_below_target_speed_growing = true; //To rename
    }
  }
  else
  {
    //To review: the time to recover is too small? When we have constant small (or big) dips with 100% speed in between,
    //we immediately recover speed but then take some time to fall back... Alternatively we could have a "high attention" phase immediately after having recovered full speed
    double gain_time_delta = time_delta * (1.0 - (actual_speed / emulation_speed));
    m_time_behind_target_speed = std::max(m_time_behind_target_speed + gain_time_delta, 0.0);
    // Time at target speed (to tell if we have recovered full speed)
    //To review: this might not work perfectly with iTimingVariance which recovers speed after a small drop in speed
    static double FallbackDelta2 = 0.001;
    if (average_actual_speed >= emulation_speed - (FallbackDelta2 * emulation_speed))
    {
      if (m_time_below_target_speed_growing)
        OSD::AddMessage("m_time_below_target_speed_growing = false", 2000U, OSD::Color::GREEN);
      m_time_below_target_speed_growing = false;
      m_time_behind_target_speed = 0;
    }
  }
  if (m_time_below_target_speed_growing)
  {
    //To review min. Also, maybe just use the same speed the frame_limiter would above?
    target_speed = frame_limiter ? std::min(average_actual_speed, emulation_speed) : average_actual_speed;
    INFO_LOG(AUDIO, "                              actual_speed: %f               average_actual_speed: %f", actual_speed, average_actual_speed);
    m_time_at_custom_speed = m_time_at_custom_speed + time_delta;
  }
  else if (frame_limiter)
  {
    m_time_at_custom_speed = 0.0;
  }

  //To test on GC with weird sample rates again. And with DVD Streaming
  //To test latency/buffer buildup (auto adjustment/sync) when we pass from stretching to not stretching. If they are always on the edge... We'll need to slow it down. Have a mirrored version of max_latency (min)?
  //To review: this should be done per mixer
  double latency;
  // The target latency used to be iTimingVariance if stretching was off and
  // m_audio_stretch_max_latency if it was on. In the first case, the reason was to cache enough
  // samples to be able to withstand small hangs and changes in speed, but our new approach is to
  // play samples backwards when we are out of new ones so these are never problems.
  double max_latency = Config::Get(Config::MAIN_AUDIO_MIXER_MAX_LATENCY) / 1000.0; //To read as little times as possible
  //To double or disable when speed is unlimited (or if we can't reach target speed), to avoid constant fluctuations (the average speed will compensate temporary imprecisions anyway)
  if (!frame_limiter || m_time_below_target_speed_growing)
  {
    static double mult = 1.0;
    max_latency *= mult;
  }
  double catch_up_speed;
  double target_latency;

  if (stretching)
  {
    // If we are reading samples at a slower speed than what they are being pushed, the stretcher
    // would keep stacking them forever, so we need to speed up. Given that samples are produced
    // in batches, we need to account for a minimum accepted latency. The stretcher latency is
    // already post mix. Normal mixers latency does not exist as they are all processed
    // immediately. Not that this isn't the whole stretcher latency, there is also the unprocessed
    // part which we can't control
    latency = m_stretcher.GetProcessedLatency();
    double acceptable_latency = m_stretcher.GetAcceptableLatency() - time_delta;
    target_latency = acceptable_latency + max_latency * 0.5;
    max_latency += acceptable_latency;
    // When we are pitch correcting it's harder to hear the change so correct it faster
    catch_up_speed = 1.25;
  }
  else
  {
    // Latency should be based on how many samples left we will have after the mixer has run
    // (predicted), not before. Otherwise if there is a sudden change of speed in between mixes,
    // or if the samples pushes and reads are done with very different timings, the latency won't
    // be stable at all and we will end up constantly adjusting it towards a value that makes no
    // sense. Also, this way we can target a latency of 0
    double rate = m_dma_mixer.GetInputSampleRate() * target_speed / m_sample_rate;
    s32 post_mix_samples = m_dma_mixer.NumSamples();
    post_mix_samples -= num_samples * rate + INTERP_SAMPLES;
    latency = std::max(post_mix_samples, 0) / m_dma_mixer.GetInputSampleRate();
    // This isn't big enough to notice but it is enough to make a difference and recover latency
    catch_up_speed = 1.015; //To review: this will be very slow if we are going at 0.1 speed already, maybe consider adding it up?
    target_latency = max_latency * 0.5;
  }
  //INFO_LOG(AUDIO, "latency: %lf", latency);

  // Instead of constantly adjusting the playback speed to be as close as possible to the target
  // latency as we did before (which lowers quality due to fluctuations), we now have a latency
  // tolerance, and while it is self adjusting when it goes too low, we need to make sure it
  // doesn't go too high. So when it goes over the limit, we speed up the playback by a very small
  // amount, almost unnoticeable, until we will have reached the target latency again.
  // The only downside of having a variable latency is in music games, where you need to
  // press a button when you hear a sound
  if (latency > (m_latency_catching_up ? target_latency : max_latency))
  {
    m_latency_catching_up = true;
    target_speed *= catch_up_speed;
    OSD::AddMessage("Reached max latency", 0U);
  }
  else
  {
    m_latency_catching_up = false;
  }

  if (stretching)
  {
    if (!m_stretching)
    {
      m_stretcher.Clear();
      m_stretching = true;
    }
    // Reset the average inside if we are predicting the audio speed, as we need it as up to date
    // as possible
    m_stretcher.SetTempo(target_speed, predicting);

    u32 available_samples = std::min(m_dma_mixer.AvailableSamples(), m_streaming_mixer.AvailableSamples());
    for (u8 i = 0; i < 4; ++i)
    {
      if (m_wiimote_speaker_mixer[i].IsCurrentlyPushed())
      {
        available_samples =
            std::min(available_samples, m_wiimote_speaker_mixer[i].AvailableSamples());
      }
    }

    bool scratch_buffer_equal_samples = m_scratch_buffer.data() == samples;

    // If the input and output sample rates difference is too high, available_samples might over the max
    m_scratch_buffer.reserve(available_samples * NC);
    if (scratch_buffer_equal_samples)
    {
      samples = m_scratch_buffer.data();  // m_scratch_buffer might have been re-allocated
      memset(samples, 0, std::max(available_samples, num_samples) * NC * sizeof(samples[0]));
    }
    else
    {
      memset(samples, 0, num_samples * NC * sizeof(samples[0]));
      memset(m_scratch_buffer.data(), 0, available_samples * NC * sizeof(m_scratch_buffer[0]));
    }

    m_dma_mixer.Mix(m_scratch_buffer.data(), available_samples, true);
    m_streaming_mixer.Mix(m_scratch_buffer.data(), available_samples, true);
    m_wiimote_speaker_mixer[0].Mix(m_scratch_buffer.data(), available_samples, true);
    m_wiimote_speaker_mixer[1].Mix(m_scratch_buffer.data(), available_samples, true);
    m_wiimote_speaker_mixer[2].Mix(m_scratch_buffer.data(), available_samples, true);
    m_wiimote_speaker_mixer[3].Mix(m_scratch_buffer.data(), available_samples, true);

    m_stretcher.PushSamples(m_scratch_buffer.data(), available_samples);
    m_stretcher.GetStretchedSamples(samples, num_samples);
  }
  else
  {
    memset(samples, 0, num_samples * NC * sizeof(samples[0]));

    m_dma_mixer.Mix(samples, num_samples, false);
    m_streaming_mixer.Mix(samples, num_samples, false);
    m_wiimote_speaker_mixer[0].Mix(samples, num_samples, false);
    m_wiimote_speaker_mixer[1].Mix(samples, num_samples, false);
    m_wiimote_speaker_mixer[2].Mix(samples, num_samples, false);
    m_wiimote_speaker_mixer[3].Mix(samples, num_samples, false);

    m_stretching = false;
  }

  return num_samples;
}

u32 Mixer::MixSurround(float* samples, u32 num_samples)
{
  memset(samples, 0, num_samples * SURROUND_CHANNELS * sizeof(samples[0]));
  
  //To clear on settings changed (num_samples): m_surround_decoder.Clear() (thread safe?)
  //To fix QuerySamplesNeededForSurroundOutput doesn't work with num_samples 0 or small
  size_t needed_samples = m_surround_decoder.QuerySamplesNeededForSurroundOutput(num_samples);

  // If we set our latency too high, we might need more samples than we have,
  // as the surround decoder can only accept exactly "needed_samples"
  m_scratch_buffer.reserve(needed_samples * NC);

  // Time stretching can be applied before decoding 5.1, it should be fine theoretically (untested).
  // Mix() may also use m_scratch_buffer internally, but is safe because we alternate reads
  // and writes. It returns the actual number of computed samples, which might be less
  // than the required ones, but as long as it computed something (it's not just all padding)
  // then we should use it for surround, otherwise these sounds would be missed
  size_t available_samples = Mix(m_scratch_buffer.data(), u32(needed_samples));
  //To fix: we can't reach this at lower latencies? Or with stretching on
  if (available_samples != needed_samples)
  {
    ERROR_LOG_FMT(AUDIO, "Error decoding surround frames.");
    return 0;
  }

  m_surround_decoder.PushSamples(m_scratch_buffer.data(), needed_samples);
  m_surround_decoder.GetDecodedSamples(samples, num_samples);

  return num_samples;
}

void Mixer::MixerFifo::PushSamples(const s16* samples, u32 num_samples)
{
  if (!samples || num_samples == 0)  // nullptr can happen
    return;

  // Cache access in non-volatile variable
  // indexR isn't allowed to cache in the audio throttling loop as it
  // needs to get updates to not deadlock.
  u32 indexW = m_indexW.load();

  u32 fifo_samples = SamplesDifference(indexW, m_indexR.load());

  // Check if we have enough free space. Accepting new samples if we haven't played back current
  // ones wouldn't make sense, it's cheaper to lose the new ones.
  // indexW == indexR results in empty buffer, so indexR must always be smaller than indexW.
  // Checking if we received 0 samples to early out is not worth it as it only happens on startup.
  if (num_samples * NC + fifo_samples > MAX_SAMPLES * NC)
  {
    static bool PrintRunOutOfSamples = false;
    if (PrintRunOutOfSamples)
      OSD::AddMessage("Run out of samples", 100, OSD::Color::RED);
    //WARN_LOG(AUDIO, "Too many samples: %i", num_samples * NC + fifo_samples - (MAX_SAMPLES * NC));
    // Fallback to the max we can currently take
    num_samples = MAX_SAMPLES - fifo_samples / NC;
  }

  // AyuanX: Actual re-sampling work has been moved to sound thread
  // to alleviate the workload on main thread
  // and we simply store raw data here to make fast mem copy
  constexpr u32 size = sizeof(s16);
  int over_bytes = (num_samples * NC - (MAX_SAMPLES * NC - (indexW & INDEX_MASK))) * size;

  if (over_bytes > 0)
  {
    auto bytes = num_samples * NC * size - over_bytes;
    memcpy(&m_buffer[indexW & INDEX_MASK], samples, bytes);
    memcpy(&m_buffer[0], samples + bytes / size, over_bytes);
  }
  else
  {
    memcpy(&m_buffer[indexW & INDEX_MASK], samples, num_samples * NC * size);
  }

  m_indexW.fetch_add(num_samples * NC);
}

void Mixer::PushDMASamples(const s16* samples, u32 num_samples)
{
  assert(num_samples > 0); //To delete
  // Use the DMA samples to determine the emulation speed, the Streaming/DVD
  // samples submissions are more frequent, so we could get more precision from
  // them, but it's really not necessary. Also, DVD is not really used on Wii
  // so it could potentially be disabled for Wii in the future.
  // Note that emulation speed can vary a lot, between frame but also between a frame,
  // as a frame might take very little, there will be a long sleep before the next one
  // starts. And if there is a speed drop, Dolphin will try to catch up (rebounce) to 
  // reach 100% speed within iTimingVariance milliseconds.
  // For these two reasons, checking individual DMA submission times makes little sense,
  // it's best to take the average of at least iTimingVariance ms (40 by default)
  // We could also rely on an external, more reliable way of calculating the emulation speed (like vblank) but they might be less accurate and I wanted to keep everything here.
  // Note that the speed at the beginning and very end of the emulation is wrong because we are receiving silent samples at random rates (depends on your CPU speed).
  // No sound will be there to be played anyway so the playback speed does not matter there.
  // When hitting a breakpoint or stopping the process in any other way, the timer here will keep running,
  // so we could easily either clamp the results within an accepted range, or compare it against another timer (e.g. from the mixer thread),
  // and see if that had a massive time between the last 2 calls, and if so, scale the second from the first
  m_dma_speed.Update(num_samples);
  m_dma_speed.CacheAverageSpeed(false); //To test different lengths
  // This average will be slightly outdated when retrieved later as m_time_at_custom_speed
  // could have increased in the meanwhile, but it's ok, the error is small enough
  m_dma_speed.CacheAverageSpeed(true, m_time_at_custom_speed);
  
  bool PrintPushedSamples = true; //To delete
  if (PrintPushedSamples) INFO_LOG(AUDIO, "dma_mixer added samples: %u, speed: %lf", num_samples, m_dma_speed.GetCachedAverageSpeed());
  m_dma_mixer.PushSamples(samples, num_samples);

  int sample_rate = m_dma_mixer.GetRoundedInputSampleRate();
  if (m_log_dsp_audio)
    m_wave_writer_dsp.AddStereoSamplesBE(samples, num_samples, sample_rate);
}

void Mixer::PushStreamingSamples(const s16* samples, u32 num_samples)
{
  bool PrintPushedSamples = false; //To delete
  if (PrintPushedSamples) INFO_LOG(AUDIO, "streaming_mixer added samples: %u", num_samples);
  m_streaming_mixer.PushSamples(samples, num_samples);

  //To review: this mixer varies a lot in num_samples?
  // Check whether the wii mote speaker mixers have finished pushing. We do it
  // from this mixer as it's the one with the higher update frequency.
  // Yes, it's a bit of a hack but it works fine
  double time_delta = double(num_samples) / m_dma_mixer.GetInputSampleRate();
  m_wiimote_speaker_mixer[0].UpdatePush(-time_delta);
  m_wiimote_speaker_mixer[1].UpdatePush(-time_delta);
  m_wiimote_speaker_mixer[2].UpdatePush(-time_delta);
  m_wiimote_speaker_mixer[3].UpdatePush(-time_delta);

  int sample_rate = m_streaming_mixer.GetRoundedInputSampleRate();
  if (m_log_dtk_audio)
    m_wave_writer_dtk.AddStereoSamplesBE(samples, num_samples, sample_rate);
}

void Mixer::PushWiimoteSpeakerSamples(u8 index, const s16* samples, u32 num_samples,
                                      u32 sample_rate)
{
  num_samples = std::min(num_samples, MAX_SAMPLES);

  m_wiimote_speaker_mixer[index].SetInputSampleRate(sample_rate);

  m_wiimote_speaker_mixer[index].UpdatePush(double(num_samples) / sample_rate);

  for (u32 i = 0; i < num_samples; ++i)
  {
    // Wii mote speaker samples are mono and aren't swapped like the other, so we pre-swap them
    m_conversion_buffer[i * NC] = Common::swap16(samples[i]);
    m_conversion_buffer[i * NC + 1] = m_conversion_buffer[i * NC];
  }

  m_wiimote_speaker_mixer[index].PushSamples(m_conversion_buffer, num_samples);
}

void Mixer::SetDMAInputSampleRate(double rate)
{
  m_dma_mixer.SetInputSampleRate(rate);
  m_dma_speed.SetTicksPerSecond(rate);
}

void Mixer::SetStreamingInputSampleRate(double rate)
{
  m_streaming_mixer.SetInputSampleRate(rate);
}

void Mixer::SetStreamingVolume(u32 lVolume, u32 rVolume)
{
  m_streaming_mixer.SetVolume(lVolume, rVolume);
}

void Mixer::SetWiimoteSpeakerVolume(u8 index, u32 lVolume, u32 rVolume)
{
  m_wiimote_speaker_mixer[index].SetVolume(lVolume, rVolume);
}

void Mixer::StartLogDTKAudio(const std::string& filename)
{
  if (!m_log_dtk_audio)
  {
    bool success = m_wave_writer_dtk.Start(filename, m_streaming_mixer.GetRoundedInputSampleRate());
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
    bool success = m_wave_writer_dsp.Start(filename, m_dma_mixer.GetRoundedInputSampleRate());
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

void Mixer::MixerFifo::DoState(PointerWrap& p)
{
  p.Do(m_input_sample_rate);
  p.Do(m_lVolume);
  p.Do(m_rVolume);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    // Restore fract to a neutral value
    m_fract = -1.0;
  }
}

void Mixer::MixerFifo::SetInputSampleRate(double rate)
{
  // We should theoretically play all the current samples at the old sample rate,
  // but the reality of that happening on real hardware when there are non zero
  // samples is pretty low if not impossible
  m_input_sample_rate = rate;
  //To review: this happens every frame it's called for the wii mote speaker
  //To review: if in and out are the same but fract has been polluted by a small change in speed once, should we reset it?
  // If we passed from an input sample rate different from the output one to
  // one that is identical, "fract" would permanently stay offsetted, which we don't really want.
  // This is likely to only happen before the game has actually played any sounds anyway
  if (m_constantly_pushed)
    m_fract = -1.0;
}

double Mixer::MixerFifo::GetInputSampleRate() const
{
  return m_input_sample_rate;
}
// For places that don't support floating point sample rates
u32 Mixer::MixerFifo::GetRoundedInputSampleRate() const
{
  return round(m_input_sample_rate);
}

void Mixer::MixerFifo::SetVolume(u32 lVolume, u32 rVolume)
{
  m_lVolume.store(lVolume + (lVolume >> 7));
  m_rVolume.store(rVolume + (rVolume >> 7));
}

u32 Mixer::MixerFifo::AvailableSamples() const
{
  u32 fifo_samples = NumSamples();
  // Mixer::MixerFifo::Mix always keeps some sample in the buffer, we want to ignore them
  if (fifo_samples <= INTERP_SAMPLES)
    return 0;
  return (fifo_samples - INTERP_SAMPLES) * m_mixer->m_sample_rate / m_input_sample_rate;
}

u32 Mixer::MixerFifo::NumSamples() const
{
  return SamplesDifference(m_indexW.load(), m_indexR.load()) / NC;
}

u32 Mixer::MixerFifo::SamplesDifference(u32 indexW, u32 indexR) const
{
  // We can't have more than MAX_SAMPLES, if we do, we loop over
  u32 diff = indexW - GetNextIndexR(indexR);
  u32 normalized_diff = diff & INDEX_MASK;
  return normalized_diff == 0u ? (diff == 0u ? 0u : (MAX_SAMPLES * NC)) : normalized_diff;
}

u32 Mixer::MixerFifo::GetNextIndexR(u32 indexR) const
{
  double rate = (m_input_sample_rate * m_mixer->GetCurrentSpeed()) / m_mixer->m_sample_rate;
  return indexR + (m_fract >= 0.0 ? NC * u32(m_fract + rate) : 0.0);
}

void Mixer::MixerFifo::UpdatePush(double time)
{
  bool currently_pushed;

  // Stop this mixer if it hasn't been pushed within the expected time
  if (time >= 0.0)
  {
    m_last_push_timer = std::max(m_last_push_timer, time);
    currently_pushed = m_last_push_timer > 0.0;
  }
  // Make sure at least 2 updates elapse before disabling "m_currently_pushed",
  // as if the negative update time is sporadic but larger, it might stop before
  // enough time has actually elapsed
  else if (m_last_push_timer > 0.0)
  {
    m_last_push_timer += time;
    currently_pushed = true;
  }
  else
  {
    currently_pushed = false;
  }

  if (m_currently_pushed != currently_pushed)
  {
    m_currently_pushed = currently_pushed;
    if (m_currently_pushed)
    {
      //To do more here? e.g. save the last playable backwards sample... (maybe, not easy to do)
      //To now increase indexR by 1 or 3 or 4? Otherwise it would be slightly delayed, but I guess it's fine as it would be the same case even if we didn't add samples...
      //Make sure the first read sample (index) will be the first pushed one
      //m_indexR.fetch_add((INTERP_SAMPLES + 1) * NC);
      //To review: add some silent samples to make sure we don't run out of samples. The target latency precisely.
      // When stretching, we don't exactly need to add a latency on top to make sure it won't run out of samples,
      // but we do to keep consistency (actually, don't do it! It add useless latency but what was the other problem??? Loss of sync between mixers? Just being useless?)
      //To review: this works originally but then it breaks after enabling and disabling stretching???
      //To review: make sure the post mix latency is correct. We can't know how many samples the backend will read and we can't know how many more samples we will receive before the next
      //backend read, but we approximate... It should be fine
      // Note that this wouldn't work if the backend/surround (mix) latency (requested samples) was too high, as you'd run out of samples in the first mix after this,
      // as this possibly started towards the end of 2 mix calls, and it hasn't build up enough samples. 3 alternative solutions:
      // -Find a way to always push silent wii mote speaker samples when the wii mote is not pushing any (e.g. from the DVD Stream push)
      // -Slow down the first mix of this any time we start pushing, to the point where it would only exactly play the number of samples available,
      //    to not incur in padding for the next audio frames
      // -Predict the next mix accuracy (by calling queryNofFra...) and setting that as min latency, maybe also check how long it has elapsed since the last mix call,
      //    to know whether we'd manage to fill it up already with the next wii mote speaker samples pushes (assuming the minimum length of a wii mote speaker push is like 0.2ms).
      //    Then using the min between the backend latency and the target latency. This might result in going over the max latency so it would speed up the sound speed
      // The wii mote pushes 40 samples at 6000Hz, which means 2.5 submissions per frame at 60fps. After pressing the input responsible for triggering samples, at least 2 batches of samples will be pushed in the next frame, though the mixing could happen at any time...
      // On original HW, might have been turned off on the last sample, meaning that if the sample wasn't 0, it wouldn't have made any additional sounds
      //To review: could we use the: Wii Mote Speaker enabled/disabled information in any way?
      if (!SConfig::GetInstance().m_audio_stretch)
      {
        u32 num_samples =
            (Config::Get(Config::MAIN_AUDIO_MIXER_MAX_LATENCY) / 1000.0) * 0.5 * m_input_sample_rate;
        num_samples = std::min(num_samples, MAX_SAMPLES);
        memset(m_mixer->m_conversion_buffer, 0,
               num_samples * NC * sizeof(m_mixer->m_conversion_buffer[0]));
        PushSamples(m_mixer->m_conversion_buffer, num_samples);
      }
    }
    else
    {
      //To do this: increase indexW by 4 more every time, with 4 samples of silence, if when new
      //samples are pushed, we haven't finished reading (indexR), then we take out the last 4 samples
      //(doesn't work with sound stretching). Or save a bool which will increase indexR by 4

      OSD::AddMessage("last sample: " + std::to_string(m_buffer[(m_indexW - 2) & INDEX_MASK]), 0U);

      constexpr u32 num_samples = INTERP_SAMPLES + 1;
      // Add enough samples of silence to make sure when it has finished reading it won't stop on a
      // non zero sample (which would break padding)
      s16 silent_samples[num_samples * NC]{};
      PushSamples(silent_samples, num_samples);
    }
  }
}
