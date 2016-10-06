// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <random>

#include <Common/CommonTypes.h>

class Dither
{
public:
  Dither() { m_mersenne_twister.seed(std::random_device{}()); }
  ~Dither() {}
  void ProcessStereo(const float* input, s16* output, u32 num_samples)
  {
    for (u32 i = 0; i < num_samples * 2; i += 2)
    {
      DitherStereoSample(&input[i], &output[i]);
    }
  }

  // function also clamps inputs to -1, 1
  virtual void DitherStereoSample(const float* in, s16* out) = 0;

protected:
  float GenerateNoise() { return m_real_dist(m_mersenne_twister); }
  static float ScaleFloatToInt(float in)
  {
    return (in > 0) ? (in * (float)0x7fff) : (in * (float)0x8000);
  }

  std::mt19937 m_mersenne_twister;
  std::uniform_real_distribution<float> m_real_dist{-0.5, 0.5};
};

class TriangleDither : public Dither
{
public:
  TriangleDither() : Dither() {}
  ~TriangleDither() {}
  void DitherStereoSample(const float* in, s16* out);

private:
  float state_l = 0, state_r = 0;
};

class ShapedDither : public Dither
{
public:
  ShapedDither() : Dither() {}
  ~ShapedDither() {}
  void DitherStereoSample(const float* in, s16* out);

private:
  int m_phase = 0;
  static constexpr int MASK = 7;
  static constexpr int SIZE = 8;
  static constexpr float FIR[] = {2.033f, -2.165f, 1.959f, -1.590f, 0.6149f};
  std::array<float, SIZE> m_buffer_l{};
  std::array<float, SIZE> m_buffer_r{};
};