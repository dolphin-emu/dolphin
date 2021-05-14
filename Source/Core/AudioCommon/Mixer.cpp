// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/Mixer.h"

#include <algorithm>
#include <cassert> //To delete
#include <climits>
#include <cmath>
#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Common/Timer.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "Common/Logging/Log.h" //To delete and all the uses (or make debug log)
#include "VideoCommon/OnScreenDisplay.h" //To delete and all the uses
#pragma optimize("", off) //To delete

Mixer::Mixer(u32 sample_rate)
    : m_sample_rate(sample_rate), m_stretcher(sample_rate),
      m_surround_decoder(sample_rate)
{
  m_scratch_buffer.reserve(MAX_SAMPLES * NC);
  m_dma_speed.Start(true);

  // These settings can't change at runtime, there aren't exposed to the UI
  m_min_latency = Config::Get(Config::MAIN_AUDIO_MIXER_MIN_LATENCY) / 1000.0;
  m_max_latency = Config::Get(Config::MAIN_AUDIO_MIXER_MAX_LATENCY) / 1000.0;

  INFO_LOG_FMT(AUDIO, "Mixer is initialized");

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
  // It would be nice to call m_dma_speed.Start(true) if m_dma_speed and paused are false,
  // but it doesn't seem to be thread safe (needs more investigation, but it's not necessary)
  m_dma_speed.SetPaused(paused);
  m_last_mix_time = Common::Timer::GetTimeUs();
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

void Mixer::UpdateSettings(u32 sample_rate)
{
  // Theoretically, we should change the playback rate of the currently buffered samples
  // to not hear a different in pitch, but it's a minor thing
  m_sample_rate = sample_rate;
  m_stretcher.SetSampleRate(m_sample_rate);
  m_surround_decoder.Init(m_sample_rate);
  m_last_mix_time = Common::Timer::GetTimeUs();
}

// -Render num_samples sample pairs to samples[]
// -Advance indexR with by the amount read
// -Return the new number of samples mixed (not the ones played backwards nor padded)
u32 Mixer::MixerFifo::Mix(s16* samples, u32 num_samples, bool stretching)
{
  // Cache access in non-volatile variable
  // This is the only function changing the read value, so it's safe to
  // cache it locally although it's written here.
  // The writing pointer will be modified outside, but it will only increase,
  // so we will just ignore new written data while interpolating.
  // Without this cache, the compiler wouldn't be allowed loops.
  u32 indexR = m_indexR.load();
  u32 indexW = m_indexW.load();

  double input_sample_rate = m_input_sample_rate.load();

  // The rate can be any, unfortunately we don't apply an anti aliasing filer, which means
  // we might get aliasing at higher rates (unless our mixer sample rate is very high)
  double rate = (input_sample_rate * m_mixer->GetMixingSpeed()) / m_mixer->m_sample_rate;

  if (!stretching)
  {
    // Latency should be based on how many samples left we will have after the mixer has run
    // (predicted), not before. Otherwise if there is a sudden change of speed in between mixes,
    // or if the samples pushes and reads are done with very different timings, the latency won't
    // be stable at all and we will end up constantly adjusting it towards a value that makes no
    // sense. Also, this way we can target a latency of "0"
    s32 post_mix_samples = SamplesDifference(indexW, indexR, rate, m_fract.load()) / NC;
    post_mix_samples -= num_samples * rate + INTERP_SAMPLES;
    double latency = std::max(post_mix_samples, 0) / input_sample_rate;
    INFO_LOG_FMT(AUDIO, "latency: {}", latency);
    // This isn't big enough to notice but it is enough to make a difference and recover latency

    AdjustSpeedByLatency(latency, 0.0, m_mixer->GetMinLatency(), m_mixer->GetMaxLatency(),
                         NON_STRETCHING_CATCH_UP_SPEED, rate, m_latency_catching_up_direction);
  }
  else
  {
    m_latency_catching_up_direction = 0;
  }

  s32 lVolume = m_lVolume.load();
  s32 rVolume = m_rVolume.load();

  s32 s[NC];  // Padding samples
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

  // I can't tell if this is better than padding (silence) when going at about 40% of the target speed
  static bool enable_backwards = true; //To review
  s32 behind_samples = num_samples - actual_samples_count;
  // This might sound bad if we are constantly missing a few samples, but that should never happen,
  // and we couldn't predict it anyway (we should start playing backwards as soon as we can).
  // We can't play backwards mixers that are not constantly pushed as we don't know when the
  // last sound started (we could not, but it's not worth implementing).
  // If required, this could be disabled from a config
  if (behind_samples > 0 && !stretching && m_constantly_pushed)
  {
    rate = input_sample_rate / m_mixer->m_sample_rate; //To review (this should actually follow the rate but with no prediction...)
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
    CubicInterpolation(back_samples, enable_backwards * behind_samples, rate, m_backwards_indexR,
                       indexW, s[0], s[1], lVolume, rVolume, false);
  }
  // Padding (constantly pushing the last sample when we run out to avoid sudden changes in the
  // audio wave). This is only needed on mixers that don't constantly push but are currently pushing,
  // as they can't play samples backwards
  else if (behind_samples > 0 && (m_constantly_pushed || m_currently_pushed))
  {
    if (indexW > 8) OSD::AddMessage("Behind samples: " + std::to_string(behind_samples), 0U);

    //To review: if we re-enable padding on wii mote forever, make sure it's disabled once we disconnect it.
    //Also, fill with padded samples when starting to push it
    unsigned int current_sample = actual_samples_count * 2;
    for (; current_sample < num_samples * 2; current_sample += 2)
    {
      samples[current_sample + 0] = std::clamp(
          samples[current_sample + 0] + m_last_output_samples[0], SHRT_MIN, SHRT_MAX);
      samples[current_sample + 1] = std::clamp(
          samples[current_sample + 1] + m_last_output_samples[1], SHRT_MIN, SHRT_MAX);
    }
  }
  //if (indexW > 8 && !m_constantly_pushed) OSD::AddMessage("last sample: " + std::to_string(m_last_output_samples[0]), 0U);

  m_indexR.store(indexR);

  return actual_samples_count;
}

// Sounds better than linear interpolation
u32 Mixer::MixerFifo::CubicInterpolation(s16* samples, u32 num_samples, double rate, u32& indexR,
                                         u32 indexW, s32& l_s, s32& r_s, s32 lVolume, s32 rVolume,
                                         bool forwards)
{
  constexpr float coeffs[] =
  { -0.5f,  1.0f, -0.5f, 0.0f,
     1.5f, -2.5f,  0.0f, 1.0f,
    -1.5f,  2.0f,  0.5f, 0.0f,
     0.5f, -0.5f,  0.0f, 0.0f };

  s8 direction = forwards ? 1 : -1;
  double fract = forwards ? m_fract.load() : m_backwards_fract;
  u32 available_samples = SamplesDifference(indexW, indexR, rate, fract);  // Forwards only
  s16* interpolation_buffer;
  if (m_big_endians)
  {
    interpolation_buffer = m_mixer->m_interpolation_buffer.data();
    u32 requested_samples = u32(rate * num_samples) * NC + NC;  // Increase by 1 for imprecisions
    u32 readable_samples = forwards ? available_samples : (MAX_SAMPLES * NC);
    u32 samples_to_read = std::min(requested_samples + (INTERP_SAMPLES * NC), readable_samples);
    u32 first_indexR = GetNextIndexR(indexR, rate, fract); //To review: this is broken with backwards fract
    u32 last_indexR = first_indexR + samples_to_read * direction;
    // Do the swaps once instead of processing them for each iteration below
    for (u32 k = first_indexR; k != last_indexR + direction * NC; k += direction * NC)
    {
      interpolation_buffer[(k + 0) & INDEX_MASK] = Common::swap16(m_buffer[(k + 0) & INDEX_MASK]);
      interpolation_buffer[(k + 1) & INDEX_MASK] = Common::swap16(m_buffer[(k + 1) & INDEX_MASK]);
    }
  }
  else
  {
    interpolation_buffer = m_buffer.data();
  }

  // fract requested to be reset so sure it will be 0 in the first cycle
  if (fract < 0.0 && num_samples > 0 && (!forwards || available_samples > INTERP_SAMPLES * NC))
  {
    fract = -rate;
  }

  u32 i = 0;
  u32 next_available_samples = available_samples;  // Forwards only
  // Stop 3 (INTERP_SAMPLES) samples from the end as we need to interpolate with them
  while (i < num_samples && (!forwards || (next_available_samples > INTERP_SAMPLES * NC &&
                                           next_available_samples <= available_samples)))
  {
    // If rate is 1 it would be like there was no interpolation.
    // If rate is 0 fract won't never make a whole so it's basically like padding.
    // If rate is a recurring decimal fract often ends up being 0.99999 due to
    // loss of precision, but that is absolutely fine, it implies no quality loss.
    // Fract errors are the reason we don't pre-calculate the number of iterations
    fract += rate;           // Update fraction position
    u32 whole = u32(fract);  // Update whole position
    fract -= whole;
    // Increase indexR before reading it, not after like before. The old code had 3 problems:
    // - IndexR was increased after being read, so if the rate was very high,
    //   it could go over indexW. This would have required a flag to be fixed
    // - In our first iteration, we would use the last fract calculated from the previous
    //   interpolation (last audio frame), meaning that it would have been based on the old
    //   rate, not the new one. Of course, over time the errors would cancel themselves out,
    //   but in the immediate, it was wrong (less reactive to changes in speed)
    // - When suddenly changing playback direction, the indexR would have been moved to the
    //   next position to read in the opposite direction
    // The only "problems" with the new code is that in the very first iteration after a fract
    // reset, fract won't be increased. It also resets the progress of indexR (fract) when we
    // run out of samples, but that won't sound any worse
    indexR += NC * whole * direction;

    if (forwards)
    {
      available_samples = next_available_samples;
      next_available_samples = SamplesDifference(indexW, indexR, rate, fract);
    }

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
    // and trade latency with a small hit in quality, but it's not worth it.
    // We could have ignored the direction while playing backwards, and just read
    // indexR over our actually play direction, but I thought that was wrong and weird
    float l_s_f = y0 * interpolation_buffer[(indexR + 1) & INDEX_MASK] +
                  y1 * interpolation_buffer[(indexR + 2 * direction + 1) & INDEX_MASK] +
                  y2 * interpolation_buffer[(indexR + 4 * direction + 1) & INDEX_MASK] +
                  y3 * interpolation_buffer[(indexR + 6 * direction + 1) & INDEX_MASK];
    float r_s_f = y0 * interpolation_buffer[indexR & INDEX_MASK] +
                  y1 * interpolation_buffer[(indexR + 2 * direction) & INDEX_MASK] +
                  y2 * interpolation_buffer[(indexR + 4 * direction) & INDEX_MASK] +
                  y3 * interpolation_buffer[(indexR + 6 * direction) & INDEX_MASK];

    // Could this benefit from multiplying the volume as a float before the round?
    l_s = (s32(std::round(l_s_f)) * lVolume) >> 8;
    r_s = (s32(std::round(r_s_f)) * rVolume) >> 8;
    // Clamp after adding to current sample, as if the cubic interpolation produced a sample over
    // the limits and the current sample has the opposite sign, then we'd keep the excess value,
    // while if it had the same sign, it would have been clamped anyway.
    // Instead of clamping to -SHORT_MAX like before to keep 0 centered, we use the full short range
    samples[i * NC + 0] = std::clamp(samples[i * NC + 0] + l_s, SHRT_MIN, SHRT_MAX);
    samples[i * NC + 1] = std::clamp(samples[i * NC + 1] + r_s, SHRT_MIN, SHRT_MAX);

    ++i;
  }

  if (forwards)
    m_fract.store(fract);
  else
    m_backwards_fract = fract;

  return i;
}

// Returns the number of samples that the backend should play.
// The samples array might not be zeroed
u32 Mixer::Mix(s16* samples, u32 num_samples, bool surround)
{
  // We can't mix if the emulation is paused as m_dma_speed would return wrong speeds.
  // We should still update the stretcher with the current speed, but num_samples == 0
  // only happens at the start and end of the emulation
  if (!samples || num_samples == 0)
    return 0;
  if (m_dma_speed.IsPaused())
  {
    std::memset(samples, 0, num_samples * NC * sizeof(samples[0]));
    return 0;
  }
  u32 original_num_samples = num_samples;

  bool stretching = SConfig::GetInstance().m_audio_stretch;

  double emulation_speed = SConfig::GetInstance().m_EmulationSpeed;
  bool frame_limiter = emulation_speed > 0.0 && !Core::GetIsThrottlerTempDisabled();

  // Backend latency in seconds
  double time_delta = double(num_samples) / m_sample_rate;
  m_backend_latency = time_delta;
  m_last_mix_time = Common::Timer::GetTimeUs();

  double average_actual_speed = m_dma_speed.GetCachedAverageSpeed(false, true, true);
  bool predicting = true;
  // Set predicting to false if we are not predicting (meaning the last samples push isn't late)
  double actual_speed = m_dma_speed.GetLastSpeed(predicting, true);
  //INFO_LOG_FMT(AUDIO, "dma_mixer current speed: {}", average_actual_speed);

  double target_speed = emulation_speed;

  if (!frame_limiter)
  {
    target_speed = m_dma_speed.GetCachedAverageSpeed(true, true, true);
    m_time_at_custom_speed = m_time_at_custom_speed + time_delta;
    // We've managed to reach the target speed so clear the m_behind_target_speed state
    if (target_speed >= emulation_speed)
    {
      m_time_behind_target_speed = 0.0;
      m_behind_target_speed = false;
    }
  }
  // Actual_speed varies by about 0.5% every audio frame so we need to go back and forth and then when we have lost enough,
  // we will start using the average actual speed. To come back at full emulation speed, we check for how long we had been
  // running at full speed, we don't wait for m_time_behind_target_speed to come back to 0 because that will never happen,
  // what's lost is lost (though recovery could still happen if for some reason we had cycles imprecisions within a frame???)
  // Stop using emulation speed, start using actual speed for audio playback, if we fell behind enough
  // Filter out small inaccuracies (samples aren't submitted with perfect timing)
  else
  {
    const double audio_emu_speed_tolerance =
        SConfig::GetInstance().m_audio_emu_speed_tolerance / 1000.0;
    const bool dynamic_audio_speed_allowed = audio_emu_speed_tolerance >= 0.0;
    const bool dynamic_audio_speed_forced = audio_emu_speed_tolerance == 0.0;

    const double gain_time_delta = time_delta * (1.0 - (actual_speed / emulation_speed));
    m_time_behind_target_speed = std::max(m_time_behind_target_speed + gain_time_delta, 0.0);

    // What we likely want to do is check the emulation speed difference between the target speed and the averaged emulation speed,
    // but then use the last frame emulation speed for more accuracy (and clamping it to the target speed)
    static double FallbackDelta = 0.0;  //0.005 or before 0.1 //To review and rename both
    static double FallbackDelta2 = 0.001;
    // Only do if we are actually going slower, if we are going faster, it's likely to be a slight incorrection of the actual speed (because it's averaged).
    // Given that we ignore the frames where we went faster, overall we will consume less audio samples that what we have received, causing less padding.
    // This avoids missing (or padded) samples when we can't reach full speed. The delta is relative to the emulator target speed.
    // When using the very last frame, the sound works much better and has less crackling and skips, at the cost of fluctuating in pitch a bit,
    // just like the emulation itself is doing.
    // NonStretchCorrectionTolerance: we don't want to suddenly snap in and out of actual FPS speed, but only when we have confirmed the slow down is not due to a hiccup
    // Small variances are already handled by the mixing, so we don't want to take care of these cases
    if (actual_speed / emulation_speed < 1.0 - FallbackDelta)
    {
      //To make sure you got the best m_audio_emu_speed_tolerance value
      //To review: this keeps trigger and going back to normal on GC due to the uneven sample rates?
      //To review: how does this sound if we always use actual_speed with sound stretching?
      //To review: test at higher backend latencies: smooth it over time? Ignore first missed frame?
      // If we fell behind of m_time_behind_target_speed seconds of samples, start using the actual emulation speed
      if (m_time_behind_target_speed > audio_emu_speed_tolerance)
      {
        if (!m_behind_target_speed && audio_emu_speed_tolerance > 0.0)
          OSD::AddMessage("m_behind_target_speed = true", 2000U);
        m_behind_target_speed = true;
      }
    }
    // Rely on the average speed (and its length) to tell if we have recovered full speed
    //To review: this might not work perfectly with iTimingVariance which recovers speed after a small stutter/drop.
    else if (average_actual_speed >= emulation_speed - (FallbackDelta2 * emulation_speed))
    {
      if (m_behind_target_speed && audio_emu_speed_tolerance > 0.0)
        OSD::AddMessage("m_behind_target_speed = false", 2000U, OSD::Color::GREEN);
      m_behind_target_speed = false;
      // Without this, it would never come back to 0, what's lost is lost
      m_time_behind_target_speed = 0.0;
    }

    if (dynamic_audio_speed_allowed && (dynamic_audio_speed_forced || m_behind_target_speed))
    {
      //To1 review: maybe just use the same speed the frame_limiter would above? When re-enabling the frame_limiter we'll get missing samples
      //as we'd use an average too long
      static bool use_new_average = true;
      target_speed = use_new_average ? m_dma_speed.GetCachedAverageSpeed(true, true, true) : average_actual_speed;
      INFO_LOG_FMT(AUDIO, "                              actual_speed: {}               average_actual_speed: {}", actual_speed, average_actual_speed);
      m_time_at_custom_speed = m_time_at_custom_speed + time_delta;
    }
    else
    {
      m_time_at_custom_speed = 0.0;
    }
  }

  //To do: add ability to skip/throw away samples if the latency is too big, instead of stretching it or keeping the delay in
  //To re-implement some mechanism for witch if the latency is a lot higher that the max, then it's played backwards faster,
  //this is especially needed now as our buffers are larger, though it can only happen if the audio thread has got some problems.
  //If you do the above, make sure you don't go over watch you are trying to reach by clamping the speed multiplication
  //To implement negative min latency and slow down sound based on it to avoid finishing samples that frame? It should work if it only happens once in a while
  //To review the case where we go lower than min latency. Should we drastically slow down the playback speed or not? The usage of min latency isn't really
  //to prevent us running out of samples within an audio frame, but in the long run, meaning that is builds up enough samples so that
  //timing differences won't make us run out
  //To review, audio get stuck looping after stopping the process (breakpoint) for like 20 seconds? It seems like the buffer gets filled and
  //then the same number of samples are written to it as they are being read, so you never move. 
  //double latency;
  // The target latency used to be iTimingVariance if stretching was off and
  // m_audio_stretch_max_latency if it was on. In the first case, the reason was to cache enough
  // samples to be able to withstand small hangs and changes in speed, but our new approach is to
  // play samples backwards when we are out of new ones so these are never problems.
  //double max_latency = m_max_latency;
  // In case you don't want to risk playing samples backwards, or your PC is unstable, or your audio
  // backend constantly changes the number of samples it asks the mixer for, set this != from 0
  //double min_latency = m_min_latency;
  //To1 review: this doesn't seem to be needed when the frame_limiter is disabled (it does reach max latency more often, but it doesn't cause any problems), but when
  //we can't reach the target sped, this might sound bad?
  if (!frame_limiter || m_behind_target_speed)
  {
    static double mult = 1.0;
    //max_latency *= mult;
  }

  if (stretching)
  {
    // If we are reading samples at a slower speed than what they are being pushed, the stretcher
    // would keep stacking them forever, so we need to speed up. Given that samples are produced
    // in batches, we need to account for a minimum accepted latency. The stretcher latency is
    // already post mix. Normal mixers latency does not exist as they are all processed
    // immediately. Not that this isn't the whole stretcher latency, there is also the unprocessed
    // part which we can't control
    const double latency = m_stretcher.GetProcessedLatency();
    INFO_LOG_FMT(AUDIO, "latency: {}", latency);
    const double acceptable_latency = m_stretcher.GetAcceptableLatency() - time_delta;
    const double min_latency = m_min_latency + acceptable_latency;
    const double max_latency = m_max_latency + acceptable_latency;
    // When we are pitch correcting it's harder to hear the change so we correct faster
    //STRETCHING_CATCH_UP_SPEED

    // Note that these changes in speed won't be "saved", so they are instantaneous,
    // we'll assume the same starting speed next frame
    AdjustSpeedByLatency(latency, acceptable_latency, min_latency, max_latency,
                         STRETCHING_CATCH_UP_SPEED, target_speed,
                         m_stretching_latency_catching_up_direction);
  }

  //To review: when this happens, we are gonna be forced to accelerate sounds as we have "surround decoder latency" of additional sounds
  //in the mixer out of the blue and need to go back to standard latency
  if (!surround && m_surround_decoder.CanReturnSamples())
  {
    // As for stretching below, this won't follow the new rate but is still better
    // than losing samples when changing settings
    u32 received_samples = m_surround_decoder.ReturnSamples(samples, num_samples);
    num_samples -= received_samples;
    samples += received_samples * NC;

    if (!m_surround_decoder.CanReturnSamples())
    {
      m_surround_decoder.Clear();
    }
  }

  m_target_speed.store(target_speed);
  m_mixing_speed.store(stretching ? 1 : target_speed);

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

    // Note that here we haven't locked the mixers m_indexR, but at worse it will only
    // increase by the time we get to the actual mixing.
    // This might also be accessing a different mixer input sample rate than we'd find
    // in the mixing, but the case where it changes is extremely rare and harmless
    u32 available_samples = std::min(m_dma_mixer.AvailableSamples(), m_streaming_mixer.AvailableSamples());
    for (u8 i = 0; i < 4; ++i)
    {
      // As long as the delay to realize the mixer has stopped pushing is lower than the stretching
      // accepted latency, this will work
      if (m_wiimote_speaker_mixer[i].IsCurrentlyPushed())
      {
        available_samples =
            std::min(available_samples, m_wiimote_speaker_mixer[i].AvailableSamples());
      }
    }

    bool scratch_buffer_equal_samples = m_scratch_buffer.data() == samples;
    
    // If the in and out sample rates difference is too high, available_samples might over the max
    m_scratch_buffer.reserve(available_samples * NC);
    if (scratch_buffer_equal_samples)
    {
      samples = m_scratch_buffer.data();  // m_scratch_buffer might have been re-allocated
      std::memset(samples, 0, std::max(available_samples, num_samples) * NC * sizeof(samples[0]));
    }
    else
    {
      std::memset(samples, 0, num_samples * NC * sizeof(samples[0]));
      std::memset(m_scratch_buffer.data(), 0, available_samples * NC * sizeof(m_scratch_buffer[0]));
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
    std::memset(samples, 0, num_samples * NC * sizeof(samples[0]));

    if (m_stretching)
    {
      m_stretching_latency_catching_up_direction = 0;
      // Play out whatever we had left. Unprocessed samples will be lost.
      // Of course this behaves weirdly when toggling stretching every audio frame,
      // and it doesn't follow the new rate, but it's better than losing samples
      u32 received_samples = m_stretcher.GetStretchedSamples(samples, num_samples, false);
      num_samples -= received_samples;
      samples += received_samples * NC;

      if (m_stretcher.GetProcessedLatency() <= 0.0)
        m_stretching = false;
    }

    //To review this (it's for surround). It can actually cause desyncs and forever increase DPLII latency, but we could fix it from ther
    u32 actual_mixed_samples = m_dma_mixer.Mix(samples, num_samples, false);
    actual_mixed_samples =
        std::max(actual_mixed_samples, m_streaming_mixer.Mix(samples, num_samples, false));
    m_wiimote_speaker_mixer[0].Mix(samples, num_samples, false);
    m_wiimote_speaker_mixer[1].Mix(samples, num_samples, false);
    m_wiimote_speaker_mixer[2].Mix(samples, num_samples, false);
    m_wiimote_speaker_mixer[3].Mix(samples, num_samples, false);
    // Note that if you don't return the padded samples in the count, some backends like OpenAL might automatically stretch the audio
    //return actual_mixed_samples;
  }

  return original_num_samples;
}

u32 Mixer::MixSurround(float* samples, u32 num_samples)
{
  std::memset(samples, 0, num_samples * SURROUND_CHANNELS * sizeof(samples[0]));
  
  // Our latency might have increased
  m_scratch_buffer.reserve(num_samples * NC);

  // TODO: we could have a special path here which mixes samples directly in float, given that the
  // cubic interpolation spits out floats

  // Time stretching can be applied before decoding 5.1, it should be fine theoretically.
  // Mix() may also use m_scratch_buffer internally, but is safe because we alternate reads
  // and writes.
  // Previously we used to immediately mix the block size of the surround decoder and then wait
  // for it to be empty again before mixing, which worked but is troublesome for many reasons:
  // - We want to try to be consistent in the number of samples we pass to the mix, as the stretching is delicate
  // - We don't want to take more than we possibly have (or before we have them at the beginning), there is a nice balance between the in and out, if we add
  //   a surround block to the game, it desyncs everything, causing latency to go up and down
  // - We don't want to write code to revert a mix that hasn't produced enough samples (unconsume the buffers),
  //   or to even ever reach that situation
  // - We want to spread the computation among frames as much as possible
  // - It simply wouldn't work well with dynamic backend latency, which is a thing. And it doesn't work well with dynamic settings, which can now be changed at runtime (like backend latency and DPLII block size). It would run out of samples in the first frame
  // - It could possibly never start playing because
  u32 mixed_samples = Mix(m_scratch_buffer.data(), num_samples, true);

  //To review: if we appened silent mix samples to the left of the buffer instead than on the right, especially the first time we start the emulation or we changed audio settings.
  //With that though, The mixer would readjust itself by playing samples quicker or slower, which could be a pro or con depending on the case (https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/AudioCommon/SurroundDecoder.cpp).
  //Also the DPLII output buffer might still get desyncronized and bigger (???).
  //To review: when enabling surround you will get half a block size of silence, should we pass in the last padded value from the stereo mixer and replace it with that?
  //To review: do we actually need to pad this? Isn't it better to let DPLII do the padding? Though we'd miss the play backwards system and the default padding. Review the comment.
  //If we suddenly push less samples for a few updates, it would be ok, it will build up again later anyway, but we might lose the perfect latency alignment...
  //Also if we don't pass in mixed_samples, when stopping you wouldn't be able to hear the last samples???
  //To review: could the 2 emulation mixers run out of sync (and get a lot desynced) with this (or with any other backend runtime setting change)?
  // To keep the backend latency aligned with the DPLII decoder latency,
  // we push in samples even if they have padded silence at the end. The alternative would be to
  // only pass in the actual samples processed by the emulation, which would avoid breaking the
  // decoder state but it would break the latency alignment.
  m_surround_decoder.PushSamples(m_scratch_buffer.data(), mixed_samples);
  // Don't get any surround sample if the Mix returned 0 as we are likely paused
  m_surround_decoder.GetDecodedSamples(samples, mixed_samples == 0 ? 0 : num_samples);

  return num_samples;
}

void Mixer::AdjustSpeedByLatency(double latency, double acceptable_latency, double min_latency,
                                 double max_latency, double catch_up_speed, double& target_speed,
                                 s8& latency_catching_up_direction)
{
  // Avoid divisions by 0 (don't rely on this, it's awful)
  if (max_latency == min_latency)
  {
    if (latency > max_latency)
      target_speed *= catch_up_speed;
    else
      target_speed /= catch_up_speed;
    return;
  }
  static bool lock = false; //To delete
  assert(!lock);
  const double target_latency = min_latency + ((max_latency - min_latency) * 0.5);
  // Instead of constantly adjusting the playback speed to be as close as possible to the target
  // latency as we did before (which lowers quality due to fluctuations), we now have a latency
  // tolerance, and while it is self adjusting when it goes too low, we need to make sure it
  // doesn't go too high. So when it goes over the limit, we speed up the playback by a very small
  // amount, almost unnoticeable, until we will have reached the target latency again.
  // The only downside of having a variable latency is in music games, where you need to
  // press a button when you hear a sound, but the variation is small enough
  if ((latency_catching_up_direction == 0 && latency > max_latency) ||
      (latency_catching_up_direction > 0 && latency > target_latency))
  {
    // Don't ever multiply the catch up speed by less than 1
    double times_over = std::max((latency - max_latency) / (max_latency - target_latency), 1.0);
    latency_catching_up_direction = 1;
    //To1 review: this will be very slow if we are going at 0.1 speed already.
    //If we add instead, we'd hear the difference more when at low speeds, and we'd catch up slower
    //at high speeds. Maybe have a minimum of catch_up_speed to apply (e.g. 0.5 speed)?
    //Multiplying is good also because you recover faster an error that is likely happening again faster (not 100% true)
    // It would be correct to somehow catch up faster if the target speed was greater than one
    // already, especially because the correction wouldn't be audible, as the pitch change hearing
    // tolerance goes one to one with the speed, but when going at very slow speeds, it would be
    // incredibly slow to catch up, so it's incorrect.
    target_speed += catch_up_speed * times_over;
    OSD::AddMessage("Reached max latency", 0U);
  }
  else if ((latency_catching_up_direction == 0 && latency < min_latency &&
            latency > acceptable_latency) ||
           (latency_catching_up_direction < 0 && latency < target_latency))
  {
    // This will likely always be 1
    double times_under = std::max((latency - min_latency) / (min_latency - target_latency), 1.0);
    latency_catching_up_direction = -1;
    //To review: this can go negative!!!
    //To review: have a more aggressive speed when under latency?
    target_speed -= catch_up_speed * times_under;
    OSD::AddMessage("Reached min latency", 0U);
  }
  else
  {
    latency_catching_up_direction = 0;
  }
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
    std::memcpy(&m_buffer[indexW & INDEX_MASK], samples, bytes);
    std::memcpy(&m_buffer[0], samples + bytes / size, over_bytes);
  }
  else
  {
    std::memcpy(&m_buffer[indexW & INDEX_MASK], samples, num_samples * NC * size);
  }

  m_indexW.fetch_add(num_samples * NC);
}

void Mixer::PushDMASamples(const s16* samples, u32 num_samples)
{
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
  m_dma_speed.CacheAverageSpeed(false); //To1 test different lengths
  // This average will be slightly outdated when retrieved later as m_time_at_custom_speed
  // could have increased in the meanwhile, but it's ok, the error is small enough
  m_dma_speed.CacheAverageSpeed(true, m_time_at_custom_speed);

  static bool PrintPushedSamples = true;
  if (PrintPushedSamples) INFO_LOG_FMT(AUDIO, "dma_mixer added samples: {}, speed: {}", num_samples, m_dma_speed.GetCachedAverageSpeed());
  m_dma_mixer.PushSamples(samples, num_samples);

  int sample_rate = m_dma_mixer.GetRoundedInputSampleRate();
  if (m_log_dsp_audio)
    m_wave_writer_dsp.AddStereoSamplesBE(samples, num_samples, sample_rate);
}

void Mixer::PushStreamingSamples(const s16* samples, u32 num_samples)
{
  m_streaming_mixer.PushSamples(samples, num_samples);

  // Check whether the wii mote speaker mixers have finished pushing. We do it
  // from this mixer as it's the one with the higher update frequency
  // (168 samples per push at 32 or 48kHz). Yes, it's a bit of a hack but it works fine
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

  // Pretend they are stereo (we should add support for mono mixers but it's a lot of code)
  for (u32 i = 0; i < num_samples; ++i)
  {
    m_conversion_buffer[i * NC] = samples[i];
    m_conversion_buffer[i * NC + 1] = samples[i];
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
      NOTICE_LOG_FMT(AUDIO, "Starting DTK Audio logging");
    }
    else
    {
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
      NOTICE_LOG_FMT(AUDIO, "Starting DSP Audio logging");
    }
    else
    {
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
}

void Mixer::MixerFifo::SetInputSampleRate(double sample_rate)
{
  // We should theoretically play all the current samples at the old sample rate,
  // but the reality of that happening when we have non silent samples is pretty low
  m_input_sample_rate = sample_rate;
}

double Mixer::MixerFifo::GetInputSampleRate() const
{
  return m_input_sample_rate;
}
// For places that don't support floating point sample rates
u32 Mixer::MixerFifo::GetRoundedInputSampleRate() const
{
  return std::round(m_input_sample_rate);
}

void Mixer::MixerFifo::SetVolume(u32 lVolume, u32 rVolume)
{
  m_lVolume.store(lVolume + (lVolume >> 7));
  m_rVolume.store(rVolume + (rVolume >> 7));
}

u32 Mixer::MixerFifo::AvailableSamples() const
{
  u32 fifo_samples = NumSamples();
  // Interpolation always keeps some sample in the buffer, we want to ignore them
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
  double rate = (m_input_sample_rate * m_mixer->GetMixingSpeed()) / m_mixer->m_sample_rate;
  return SamplesDifference(indexW, indexR, rate, m_fract.load());
}
u32 Mixer::MixerFifo::SamplesDifference(u32 indexW, u32 indexR, double rate, double fract) const
{
  // We can't have more than MAX_SAMPLES, if we do, we loop over
  u32 diff = indexW - GetNextIndexR(indexR, rate, fract);
  u32 normalized_diff = diff & INDEX_MASK;
  return normalized_diff == 0u ? (diff == 0u ? 0u : (MAX_SAMPLES * NC)) : normalized_diff;
}

u32 Mixer::MixerFifo::GetNextIndexR(u32 indexR, double rate, double fract) const
{
  //To review: this can still return an indexR greater than indexW... Stupid. Maybe save that is_finished bool anyway?
  return indexR + (fract >= 0.0 ? NC * u32(fract + rate) : 0.0);
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

  //To do: we could directly set m_currently_pushed when the wii mote speaker is muted or disabled
  if (m_currently_pushed != currently_pushed)
  {
    m_currently_pushed = currently_pushed;
    if (m_currently_pushed)
    {
      static bool without = false;
      if (without) return; //To delete
      // Add some silent samples to make sure we don't run out of samples immediately,
      // as we can't know when the mix is going to happen.
      // When stretching, we don't need to add a latency as it's not related to time.
      //To review: what if the wiimote push literally 20 samples and that's it? In that case we'd add a lot of latency for nothing? Maybe this is not worth it?
      if (!SConfig::GetInstance().m_audio_stretch)
      {
        // Use AvailableSamples() to ignore INTERP_SAMPLES
        const double current_latency =
            (time + (AvailableSamples() / m_mixer->GetSampleRate())) / m_mixer->GetCurrentSpeed();
        const double target_latency = m_mixer->GetMinLatency() +
                                      ((m_mixer->GetMaxLatency() - m_mixer->GetMinLatency()) * 0.5);
        // Try to avoid adding the whole backend latency by predicting when the next audio thread
        // mix will run
        const double backend_elapsed_time =
            (Common::Timer::GetTimeUs() - m_mixer->m_last_mix_time) / 1000000.0;
        const double backend_time_alpha =
            1.0 - std::min(backend_elapsed_time / m_mixer->m_backend_latency, 1.0);
        const double latency_to_add =
            target_latency - current_latency + (m_mixer->m_backend_latency * backend_time_alpha);

        u32 num_samples = std::round(std::max(latency_to_add, 0.0) * m_input_sample_rate);
        num_samples = std::min(num_samples, MAX_SAMPLES);
        
        std::memset(m_mixer->m_conversion_buffer, 0,
               num_samples * NC * sizeof(m_mixer->m_conversion_buffer[0]));
        PushSamples(m_mixer->m_conversion_buffer, num_samples);
      }
    }
    else
    {
      //OSD::AddMessage("last sample: " + std::to_string(m_buffer[(m_indexW - 1) & INDEX_MASK]), 0U);

      constexpr u32 num_samples = INTERP_SAMPLES + 1;
      // Add enough samples of silence to make sure when it has finished reading it won't stop on a
      // non zero sample (which would restrict the range of the other mixers).
      // Real wii motes deal with this in 2 ways: by disabling the speaker after a sound or by
      // padding the last sample.
      s16 silent_samples[num_samples * NC]{};
      PushSamples(silent_samples, num_samples);
    }
  }
}
