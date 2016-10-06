// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/WindowedSincFilter.h"

// iteratively generates zero-order modified Bessel function of the first kind
// stop when improvements fall below threshold
double WindowedSincFilter::ModBesselZeroth(const double x) const
{
  double sum = 1.0;
  s32 factorial_store = 1;
  double half_x = x / 2.0;
  double previous = 1.0;
  do
  {
    double temp = half_x / (double) factorial_store;
    temp *= temp;
    previous *= temp;
    factorial_store++;
    sum += previous;
  } while (previous >= BESSEL_EPSILON * sum);
  return sum;
}

void WindowedSincFilter::PopulateFilterDeltas()
{
  for (u32 i = 0; i < m_wing_size - 1; ++i)
  {
    m_deltas[i] = m_coeffs[i + 1] - m_coeffs[i];
  }
  m_deltas[m_wing_size - 1] = -m_coeffs[m_wing_size - 1];
}

void WindowedSincFilter::PopulateFilterCoeffs()
{
  double inv_I0_beta = 1.0 / ModBesselZeroth(m_kaiser_beta);
  double inv_size = 1.0 / (m_wing_size - 1);
  for (u32 i = 0; i < m_wing_size; ++i)
  {
    double offset = PI * (double)i / (double)m_samples_per_crossing;
    double sinc = (offset == 0.f)
                  ? (double)m_cutoff_cycle
                  : sin(offset *  m_cutoff_cycle) / offset;
    double radicand = (double)i * inv_size;
    radicand = 1.0 - radicand * radicand;
    // apply Kaiser window to sinc
    m_coeffs[i] = (float)(sinc * ModBesselZeroth(m_kaiser_beta * sqrt(radicand)) * inv_I0_beta);
  }

  // Scale so that DC signals have unity gain
  double dc_gain = 0;
  for (u32 i = m_samples_per_crossing; i < m_wing_size; i += m_samples_per_crossing)
  {
    dc_gain += m_coeffs[i];
  }
  dc_gain *= 2;
  dc_gain += m_coeffs[0];
  double inv_dc_gain = 1.0 / dc_gain;

  for (u32 i = 0; i < m_wing_size; ++i)
  {
    m_coeffs[i] = (float)(m_coeffs[i] * inv_dc_gain);
  }
}

void WindowedSincFilter::ConvolveStereo(const RingBuffer<float>& input, u32 index,
                                        float* output_l, float* output_r,
                                        const float fraction, const float ratio) const
{
  if (ratio >= 1.f)
    UpSampleStereo(input, index, output_l, output_r, fraction);
  else
    DownSampleStereo(input, index, output_l, output_r, fraction, ratio);
}

void WindowedSincFilter::UpSampleStereo(const RingBuffer<float>& input, u32 index,
                                        float* output_l, float* output_r,
                                        const float fraction) const
{
  float left_channel = 0.0, right_channel = 0.0;

  // Convolve left wing first
  float left_frac = (fraction * m_samples_per_crossing);
  u32 left_index = (u32)left_frac;
  left_frac -= (float)left_index;

  u32 current_index = index - m_num_taps;

  for (; left_index < m_wing_size; left_index += m_samples_per_crossing, current_index -= 2)
  {
    float impulse = m_coeffs[left_index];
    impulse += m_deltas[left_index] * left_frac;

    left_channel += input[current_index] * impulse;
    right_channel += input[current_index + 1] * impulse;
  }

  float right_frac = 0.f;
  u32 right_index = 0;
  if (fraction == 0.f)
  {
      right_frac = 0.f;
      right_index = m_samples_per_crossing;
  }
  else
  {
    right_frac = (1 - fraction) * m_samples_per_crossing;
    right_index = (u32)right_frac;
    right_frac -= (float)right_index;
  }

  current_index = index - m_num_taps + 2;

  for (; right_index < m_wing_size; right_index += m_samples_per_crossing, current_index += 2)
  {
    float impulse = m_coeffs[right_index];
    impulse += m_deltas[right_index] * right_frac;

    left_channel += input[current_index] * impulse;
    right_channel += input[current_index + 1] * impulse;
  }

  *output_l = left_channel;
  *output_r = right_channel;


}
void WindowedSincFilter::DownSampleStereo(const RingBuffer<float>& input, u32 index,
                                          float* output_l, float* output_r,
                                          const float fraction, const float ratio) const
{
  float left_channel = 0.0, right_channel = 0.0;

  float left_frac = (fraction * m_samples_per_crossing);
  left_frac *= ratio;
  u32 left_index = (u32)left_frac;
  left_frac -= (float)left_index;

  u32 current_index = index - m_num_taps;

  for (; left_index < m_wing_size; current_index -= 2)
  {
    float impulse = m_coeffs[left_index];
    impulse += m_deltas[left_index] * left_frac;

    left_channel += input[current_index] * impulse;
    right_channel += input[current_index + 1] * impulse;

    left_frac += (float)left_index;
    left_frac += ratio * (float)m_samples_per_crossing;
    left_index = (u32)left_frac;
    left_frac -= (float)left_index;
  }

  float right_frac = 0.f;
  u32 right_index = 0;
  if (fraction == 0.f)
  {
    right_frac = ratio * m_samples_per_crossing;
    right_index = (u32)right_index;
    right_frac -= (float)right_index;
  }
  else
  {
    right_frac = (1 - fraction) * m_samples_per_crossing;
    right_frac *= ratio;
    right_index = (u32)right_frac;
    right_frac -= (float)right_index;
  }

  current_index = index - m_num_taps + 2;

  for (; right_index < m_wing_size; current_index += 2)
  {
    float impulse = m_coeffs[right_index];
    impulse += m_deltas[right_index] * right_frac;

    left_channel += input[current_index] * impulse;
    right_channel += input[current_index + 1] * impulse;

    right_frac += (float)right_index;
    right_frac += ratio * (float)m_samples_per_crossing;
    right_index = (u32)right_frac;
    right_frac -= (float)right_index;
  }

  *output_l = left_channel;
  *output_r = right_channel;
}