// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <random>

#include "Common/CommonTypes.h"

class Dither
{
public:
  Dither() { m_mersenne_twister.seed(std::random_device{}()); }
  ~Dither() = default;
  void ProcessStereo(const float* input, s16* output, u32 num_samples);

  // function also clamps inputs to -1, 1
  virtual void DitherStereoSample(const float* in, s16* out) = 0;

protected:
  float GenerateNoise() { return m_real_dist(m_mersenne_twister); }
  static float ScaleFloatToInt(float in);

  std::mt19937 m_mersenne_twister;
  std::uniform_real_distribution<float> m_real_dist{-0.5, 0.5};
};

class TriangleDither final : public Dither
{
public:
  TriangleDither() : Dither() {}
  ~TriangleDither() = default;
  void DitherStereoSample(const float* in, s16* out) override;

private:
  float m_state_l = 0;
  float m_state_r = 0;
};

class ShapedDither final : public Dither
{
public:
  ShapedDither() : Dither() {}
  ~ShapedDither() = default;
  void DitherStereoSample(const float* in, s16* out) override;

private:
  int m_phase = 0;
  static constexpr int MASK = 7;
  static constexpr int SIZE = 8;
  static const std::array<float, 5> FIR;
  std::array<float, SIZE> m_buffer_l{};
  std::array<float, SIZE> m_buffer_r{};
};