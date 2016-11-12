// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/WindowedSincFilter.h"

#include <cmath>

// makes sure num_crossings is odd; there are no restrictions on samples_per_crossing
// cutoff frequency must be below nyquist (which we are counting as 1.0)
WindowedSincFilter::WindowedSincFilter(u32 num_crossings, u32 samples_per_crossing,
                                       float cutoff_cycle, float beta)
    : BaseFilter(num_crossings / 2), m_samples_per_crossing(samples_per_crossing),
      m_wing_size(samples_per_crossing * (num_crossings / 2)),
      m_cutoff_cycle(std::min(cutoff_cycle, 1.f)), m_kaiser_beta(beta)
{
  m_coeffs.resize(m_wing_size);
  m_deltas.resize(m_wing_size);
  PopulateFilterCoeffs();
  PopulateFilterDeltas();
}

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
    double temp = half_x / static_cast<double>(factorial_store);
    temp *= temp;
    previous *= temp;
    factorial_store++;
    sum += previous;
  } while (previous >= BESSEL_EPSILON * sum);
  return sum;
}

void WindowedSincFilter::PopulateFilterDeltas()
{
  for (size_t i = 0; i < m_wing_size - 1; ++i)
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
    double offset = PI * static_cast<double>(i) / static_cast<double>(m_samples_per_crossing);
    double sinc = (offset == 0.f) ? static_cast<double>(m_cutoff_cycle) :
                                    sin(offset * m_cutoff_cycle) / offset;
    double radicand = static_cast<double>(i) * inv_size;
    radicand = 1.0 - radicand * radicand;
    // apply Kaiser window to sinc
    m_coeffs[i] =
        static_cast<float>(sinc * ModBesselZeroth(m_kaiser_beta * sqrt(radicand)) * inv_I0_beta);
  }

  // Scale so that DC signals have unity gain
  double dc_gain = 0;
  for (size_t i = m_samples_per_crossing; i < m_wing_size; i += m_samples_per_crossing)
  {
    dc_gain += m_coeffs[i];
  }
  dc_gain *= 2;
  dc_gain += m_coeffs[0];
  double inv_dc_gain = 1.0 / dc_gain;

  for (size_t i = 0; i < m_wing_size; ++i)
  {
    m_coeffs[i] = static_cast<float>(m_coeffs[i] * inv_dc_gain);
  }
}

void WindowedSincFilter::ConvolveStereo(const RingBuffer<float>& input, size_t index,
                                        float* output_l, float* output_r, const float fraction,
                                        const float ratio) const
{
  if (ratio >= 1.f)
    UpSampleStereo(input, index, output_l, output_r, fraction);
  else
    DownSampleStereo(input, index, output_l, output_r, fraction, ratio);
}

void WindowedSincFilter::UpSampleStereo(const RingBuffer<float>& input, size_t index,
                                        float* output_l, float* output_r,
                                        const float fraction) const
{
  float left_channel = 0.0;
  float right_channel = 0.0;

  // Convolve left wing first
  float left_frac = (fraction * m_samples_per_crossing);
  size_t left_index = static_cast<size_t>(left_frac);
  left_frac -= static_cast<float>(left_index);

  size_t current_index = index - m_num_taps;

  for (; left_index < m_wing_size; left_index += m_samples_per_crossing, current_index -= 2)
  {
    float impulse = m_coeffs[left_index];
    impulse += m_deltas[left_index] * left_frac;

    left_channel += input[current_index] * impulse;
    right_channel += input[current_index + 1] * impulse;
  }

  float right_frac = 0.f;
  size_t right_index = 0;
  if (fraction == 0.f)
  {
    right_frac = 0.f;
    right_index = m_samples_per_crossing;
  }
  else
  {
    right_frac = (1 - fraction) * m_samples_per_crossing;
    right_index = static_cast<size_t>(right_frac);
    right_frac -= static_cast<float>(right_index);
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

void WindowedSincFilter::DownSampleStereo(const RingBuffer<float>& input, size_t index,
                                          float* output_l, float* output_r, const float fraction,
                                          const float ratio) const
{
  float left_channel = 0.0;
  float right_channel = 0.0;

  float left_frac = (fraction * m_samples_per_crossing);
  left_frac *= ratio;
  size_t left_index = static_cast<size_t>(left_frac);
  left_frac -= static_cast<float>(left_index);

  size_t current_index = index - m_num_taps;

  for (; left_index < m_wing_size; current_index -= 2)
  {
    float impulse = m_coeffs[left_index];
    impulse += m_deltas[left_index] * left_frac;

    left_channel += input[current_index] * impulse;
    right_channel += input[current_index + 1] * impulse;

    left_frac += static_cast<float>(left_index);
    left_frac += ratio * static_cast<float>(m_samples_per_crossing);
    left_index = static_cast<size_t>(left_frac);
    left_frac -= static_cast<float>(left_index);
  }

  float right_frac = 0.f;
  size_t right_index = 0;
  if (fraction == 0.f)
  {
    right_frac = ratio * m_samples_per_crossing;
    right_index = static_cast<size_t>(right_index);
    right_frac -= static_cast<float>(right_index);
  }
  else
  {
    right_frac = (1 - fraction) * m_samples_per_crossing;
    right_frac *= ratio;
    right_index = static_cast<size_t>(right_frac);
    right_frac -= static_cast<float>(right_index);
  }

  current_index = index - m_num_taps + 2;

  for (; right_index < m_wing_size; current_index += 2)
  {
    float impulse = m_coeffs[right_index];
    impulse += m_deltas[right_index] * right_frac;

    left_channel += input[current_index] * impulse;
    right_channel += input[current_index + 1] * impulse;

    right_frac += static_cast<float>(right_index);
    right_frac += ratio * static_cast<float>(m_samples_per_crossing);
    right_index = static_cast<size_t>(right_frac);
    right_frac -= static_cast<float>(right_index);
  }

  *output_l = left_channel;
  *output_r = right_channel;
}