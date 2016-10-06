// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Dither.h"

#include "Common/MathUtil.h"

void TriangleDither::DitherStereoSample(const float* in, s16* out)
{
  float random_l = GenerateNoise();
  float random_r = GenerateNoise();

  float temp_l = ScaleFloatToInt(MathUtil::Clamp(in[0], -1.f, 1.f)) + random_l - state_l;
  float temp_r = ScaleFloatToInt(MathUtil::Clamp(in[1], -1.f, 1.f)) + random_r - state_r;
  out[0] = (s16)MathUtil::Clamp((s32)lrintf(temp_l), -32768, 32767);
  out[1] = (s16)MathUtil::Clamp((s32)lrintf(temp_r), -32768, 32767);
  state_l = random_l;
  state_r = random_r;
}

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

  out[0] = (s16)MathUtil::Clamp((s32)lrintf(result_l), -32768, 32767);
  out[1] = (s16)MathUtil::Clamp((s32)lrintf(result_r), -32768, 32767);

  m_phase = (m_phase + 1) & MASK;
  m_buffer_l[m_phase] = conv_l - (float)out[0];
  m_buffer_r[m_phase] = conv_r - (float)out[1];
}