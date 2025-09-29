// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>
#include "Common/CommonTypes.h"

namespace DSP::HLE
{
class DolbyProLogicII
{
public:
  DolbyProLogicII();
  ~DolbyProLogicII() = default;

  // Main encoding function
  // Takes 5 channels (L, R, C, RL, RR) and encodes them to stereo (Lt, Rt)
  // Input: 5 separate buffers for each channel, each with num_samples
  // Output: Stereo interleaved output (LRLR...)
  void Encode(const int* left, const int* right, const int* center,
              const int* rear_left, const int* rear_right,
              int* output_left, int* output_right, u32 num_samples);

  // Helper function to apply DPL2 to AX buffers based on mixer_control
  // This processes the 9 AX buffers (Main/AuxA/AuxB x L/R/S) according to game requirements
  void ProcessAXBuffers(u32 mixer_control,
                       int* main_left, int* main_right, int* main_surround,
                       int* auxa_left, int* auxa_right, int* auxa_surround,
                       int* auxb_left, int* auxb_right, int* auxb_surround,
                       u32 num_samples);

  // Reset the encoder state (clears Hilbert transform history)
  void Reset();

private:
  // Hilbert transform implementation for 90-degree phase shift
  // Uses an FIR approximation with a delay line
  class HilbertTransform
  {
  public:
    static constexpr int FILTER_LENGTH = 31;  // Odd length for symmetric filter
    static constexpr int DELAY = FILTER_LENGTH / 2;

    HilbertTransform();
    void Reset();
    float Process(float input);

  private:

    std::array<float, FILTER_LENGTH> m_delay_line{};
    int m_index = 0;
    std::array<float, FILTER_LENGTH> m_coefficients{};

    void InitializeCoefficients();
  };

  // Phase shifters for rear channels
  HilbertTransform m_hilbert_rl;
  HilbertTransform m_hilbert_rr;

  // Delay line for matching Hilbert transform latency on direct channels
  class DelayLine
  {
  public:
    DelayLine(int delay_samples);
    void Reset();
    float Process(float input);

  private:
    std::vector<float> m_buffer;
    int m_index = 0;
    int m_delay;
  };

  DelayLine m_delay_left;
  DelayLine m_delay_right;
  DelayLine m_delay_center;

  // DPL2 matrix coefficients (from documentation)
  static constexpr float MATRIX_CENTER = 0.707f;   // √(1/2)
  static constexpr float MATRIX_RL_LT = 0.866f;   // √(3/4)
  static constexpr float MATRIX_RL_RT = 0.500f;   // 1/2
  static constexpr float MATRIX_RR_LT = 0.500f;   // 1/2
  static constexpr float MATRIX_RR_RT = 0.866f;   // √(3/4)
};

}  // namespace DSP::HLE