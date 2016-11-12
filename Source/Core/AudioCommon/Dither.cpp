// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/Dither.h"

#include <cmath>

#include "Common/MathUtil.h"

void Dither::ProcessStereo(const float* input, s16* output, u32 num_samples)
{
  for (u32 i = 0; i < num_samples * 2; i += 2)
  {
    DitherStereoSample(&input[i], &output[i]);
  }
}

float Dither::ScaleFloatToInt(float in)
{
  return (in > 0) ? (in * static_cast<float>(0x7fff)) : (in * static_cast<float>(0x8000));
}

void TriangleDither::DitherStereoSample(const float* in, s16* out)
{
  float random_l = GenerateNoise();
  float random_r = GenerateNoise();

  float temp_l = ScaleFloatToInt(MathUtil::Clamp(in[0], -1.f, 1.f)) + random_l - m_state_l;
  float temp_r = ScaleFloatToInt(MathUtil::Clamp(in[1], -1.f, 1.f)) + random_r - m_state_r;
  out[0] = static_cast<s16>(MathUtil::Clamp(static_cast<s32>(lrintf(temp_l)), -32768, 32767));
  out[1] = static_cast<s16>(MathUtil::Clamp(static_cast<s32>(lrintf(temp_r)), -32768, 32767));
  m_state_l = random_l;
  m_state_r = random_r;
}

const std::array<float, 5> ShapedDither::FIR = {2.033f, -2.165f, 1.959f, -1.590f, 0.6149f};

void ShapedDither::DitherStereoSample(const float* in, s16* out)
{
  float random_l = GenerateNoise() + GenerateNoise();
  float random_r = GenerateNoise() + GenerateNoise();

  float conv_l = ScaleFloatToInt(MathUtil::Clamp(in[0], -1.f, 1.f));
  float conv_r = ScaleFloatToInt(MathUtil::Clamp(in[1], -1.f, 1.f));

  conv_l += m_buffer_l[m_phase] * FIR[0];
  conv_r += m_buffer_r[m_phase] * FIR[0];

  conv_l += m_buffer_l[(m_phase - 1) & MASK] * FIR[1];
  conv_r += m_buffer_r[(m_phase - 1) & MASK] * FIR[1];

  conv_l += m_buffer_l[(m_phase - 2) & MASK] * FIR[2];
  conv_r += m_buffer_r[(m_phase - 2) & MASK] * FIR[2];

  conv_l += m_buffer_l[(m_phase - 3) & MASK] * FIR[3];
  conv_r += m_buffer_r[(m_phase - 3) & MASK] * FIR[3];

  conv_l += m_buffer_l[(m_phase - 4) & MASK] * FIR[4];
  conv_r += m_buffer_r[(m_phase - 4) & MASK] * FIR[4];

  float result_l = conv_l + random_l;
  float result_r = conv_r + random_r;

  out[0] = static_cast<s16>(MathUtil::Clamp(static_cast<s32>(lrintf(result_l)), -32768, 32767));
  out[1] = static_cast<s16>(MathUtil::Clamp(static_cast<s32>(lrintf(result_r)), -32768, 32767));

  m_phase = (m_phase + 1) & MASK;
  m_buffer_l[m_phase] = conv_l - (float)out[0];
  m_buffer_r[m_phase] = conv_r - (float)out[1];
}