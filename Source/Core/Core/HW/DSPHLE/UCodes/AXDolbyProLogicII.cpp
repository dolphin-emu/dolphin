// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPHLE/UCodes/AXDolbyProLogicII.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

namespace DSP::HLE
{
// HilbertTransform implementation
DolbyProLogicII::HilbertTransform::HilbertTransform()
{
  InitializeCoefficients();
  Reset();
}

void DolbyProLogicII::HilbertTransform::InitializeCoefficients()
{
  // Generate Hilbert transform FIR coefficients
  // These coefficients approximate a 90-degree phase shift across the frequency spectrum
  // Using a windowed sinc function approach for the imaginary part of the analytic signal

  const int half_length = FILTER_LENGTH / 2;

  for (int i = 0; i < FILTER_LENGTH; ++i)
  {
    int n = i - half_length;
    if (n == 0)
    {
      m_coefficients[i] = 0.0f;
    }
    else if (n % 2 != 0)  // Odd values only
    {
      // Hilbert kernel: h[n] = (2/π) * sin²(πn/2) / n
      // For odd n: h[n] = 2/(πn)
      m_coefficients[i] = 2.0f / (MathUtil::PI * n);

      // Apply Hamming window for better frequency response
      float window = 0.54f - 0.46f * std::cos(2.0f * MathUtil::PI * i / (FILTER_LENGTH - 1));
      m_coefficients[i] *= window;
    }
    else
    {
      m_coefficients[i] = 0.0f;
    }
  }
}

void DolbyProLogicII::HilbertTransform::Reset()
{
  m_delay_line.fill(0.0f);
  m_index = 0;
}

float DolbyProLogicII::HilbertTransform::Process(float input)
{
  // Add input to circular buffer
  m_delay_line[m_index] = input;

  // Compute FIR convolution
  float output = 0.0f;
  for (int i = 0; i < FILTER_LENGTH; ++i)
  {
    int delay_idx = (m_index - i + FILTER_LENGTH) % FILTER_LENGTH;
    output += m_coefficients[i] * m_delay_line[delay_idx];
  }

  // Advance circular buffer index
  m_index = (m_index + 1) % FILTER_LENGTH;

  return output;
}

// DelayLine implementation
DolbyProLogicII::DelayLine::DelayLine(int delay_samples)
    : m_buffer(delay_samples, 0.0f), m_delay(delay_samples)
{
}

void DolbyProLogicII::DelayLine::Reset()
{
  std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
  m_index = 0;
}

float DolbyProLogicII::DelayLine::Process(float input)
{
  if (m_delay == 0)
    return input;

  float output = m_buffer[m_index];
  m_buffer[m_index] = input;
  m_index = (m_index + 1) % m_delay;
  return output;
}

// DolbyProLogicII implementation
DolbyProLogicII::DolbyProLogicII()
    : m_delay_left(HilbertTransform::FILTER_LENGTH / 2),
      m_delay_right(HilbertTransform::FILTER_LENGTH / 2),
      m_delay_center(HilbertTransform::FILTER_LENGTH / 2)
{
  Reset();
}

void DolbyProLogicII::Reset()
{
  m_hilbert_rl.Reset();
  m_hilbert_rr.Reset();
  m_delay_left.Reset();
  m_delay_right.Reset();
  m_delay_center.Reset();
}

void DolbyProLogicII::Encode(const int* left, const int* right, const int* center,
                             const int* rear_left, const int* rear_right,
                             int* output_left, int* output_right, u32 num_samples)
{
  // DPL2 encoding matrix:
  // Lt = L + 0.707*C + j*0.866*RL + j*0.5*RR
  // Rt = R + 0.707*C - j*0.5*RL - j*0.866*RR
  //
  // Where j represents a +90° phase shift (Hilbert transform)

  for (u32 i = 0; i < num_samples; ++i)
  {
    // Convert to float for processing
    float l = static_cast<float>(left[i]);
    float r = static_cast<float>(right[i]);
    float c = static_cast<float>(center[i]);
    float rl = static_cast<float>(rear_left[i]);
    float rr = static_cast<float>(rear_right[i]);

    // Global pre-attenuation (~-3 dB to -6 dB) to preserve headroom through the matrix.
    // We choose ~-3 dB here to maintain loudness while avoiding clipping in common cases.
    const float pre = 0.70710678f; // sqrt(1/2)
    l *= pre; r *= pre; c *= pre; rl *= pre; rr *= pre;

    // Apply Hilbert transform (90° phase shift) to rear channels
    float rl_shifted = m_hilbert_rl.Process(rl);
    float rr_shifted = m_hilbert_rr.Process(rr);

    // Apply matching delay to direct channels so group delay matches the Hilbert paths
    float l_delayed = m_delay_left.Process(l);
    float r_delayed = m_delay_right.Process(r);
    float c_delayed = m_delay_center.Process(c);

    // Apply DPL2 matrix encoding (see DPL2.md for derivation)
    float lt = l_delayed + MATRIX_CENTER * c_delayed +
               MATRIX_RL_LT * rl_shifted + MATRIX_RR_LT * rr_shifted;

    float rt = r_delayed + MATRIX_CENTER * c_delayed -
               MATRIX_RL_RT * rl_shifted - MATRIX_RR_RT * rr_shifted;

    // Convert back to integer samples (saturate to int range used by AX mixing, then 16-bit later)
    // We avoid extra clamping here; OutputSamples() will clamp to s16.
    output_left[i] = static_cast<int>(std::lrint(lt));
    output_right[i] = static_cast<int>(std::lrint(rt));
  }
}

void DolbyProLogicII::ProcessAXBuffers(u32 mixer_control,
                                      int* main_left, int* main_right, int* main_surround,
                                      int* auxa_left, int* auxa_right, int* auxa_surround,
                                      int* auxb_left, int* auxb_right, int* auxb_surround,
                                      u32 num_samples)
{
  // Based on the documentation and game behavior analysis:
  // - Rogue Squadron 2: Uses mixer_control 0x0010 and 0x0018 for DPL2
  // - When DPL2 is active, AuxB is used for surround channels

  // Check if DPL2 encoding is requested
  bool dpl2_enabled = false;

  // Check for specific mixer control patterns that indicate DPL2
  // From ConvertMixerControl in AX.cpp: 0x0010 flag indicates DPL2 mixing
  if (mixer_control & 0x0010)
  {
    // For CRC 0x4e8a8b21 (Rogue Squadron), this is DPL2 mode
    dpl2_enabled = true;
  }
  else if (mixer_control & 0x4000)
  {
    // For newer GC ucodes, 0x4000 is the DPL2 flag
    dpl2_enabled = true;
  }

  if (!dpl2_enabled)
    return;

  // Prepare 5-channel input for DPL2 encoding
  // Map the AX buffers to DPL2 channels based on game behavior:

  // In DPL2 mode for Rogue Squadron 2:
  // - Main.L/R are the front left/right channels
  // - Main.S provides the center channel (derived from L+R)
  // - AuxB.L/R are used for rear left/right

  // Allocate temporary buffers for DPL2 processing
  std::vector<int> center_channel(num_samples);
  std::vector<int> output_left(num_samples);
  std::vector<int> output_right(num_samples);

  // Derive center channel from main surround or from L+R
  for (u32 i = 0; i < num_samples; ++i)
  {
    if (main_surround[i] != 0)
    {
      // Use existing surround as center
      center_channel[i] = main_surround[i];
    }
    else
    {
      // Derive center from L+R (as per DPL2 spec: C = 0.707 * (L + R))
      center_channel[i] = static_cast<int>(0.707f * (main_left[i] + main_right[i]));
    }
  }

  // Apply DPL2 encoding
  // Using AuxB channels as rear speakers
  Encode(main_left, main_right, center_channel.data(),
         auxb_left, auxb_right,
         output_left.data(), output_right.data(),
         num_samples);

  // Write encoded output back to main channels
  std::memcpy(main_left, output_left.data(), num_samples * sizeof(int));
  std::memcpy(main_right, output_right.data(), num_samples * sizeof(int));

  // Clear the auxiliary buffers that were used for encoding
  // This prevents them from being mixed again
  std::memset(auxb_left, 0, num_samples * sizeof(int));
  std::memset(auxb_right, 0, num_samples * sizeof(int));
  std::memset(main_surround, 0, num_samples * sizeof(int));

  INFO_LOG_FMT(DSPHLE, "DPL2: Processed {} samples with mixer_control {:04x}",
               num_samples, mixer_control);
}

}  // namespace DSP::HLE