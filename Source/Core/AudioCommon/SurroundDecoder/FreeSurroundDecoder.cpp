// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

/*
Copyright (C) 2007-2010 Christian Kothe

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "AudioCommon/SurroundDecoder/FreeSurroundDecoder.h"

#include <algorithm>
#include <cstring>
#include <numbers>

using std::numbers::pi;

#undef min
#undef max
#define kiss_fftr_free free

// FreeSurround implementation
DPL2FSDecoder::DPL2FSDecoder(const channel_setup chsetup, const unsigned int blocksize,
                             const unsigned int sample_rate)
{
  setup = chsetup;
  N = blocksize;

  // Initialize the parameters
  wnd = std::vector<double>(N);
  inbuf = std::vector<float>(3 * N);
  lt = std::vector<double>(N);
  rt = std::vector<double>(N);
  dst = std::vector<double>(N);
  lf = std::vector<cplx>(N / 2 + 1);
  rf = std::vector<cplx>(N / 2 + 1);
  forward = kiss_fftr_alloc(N, 0);
  inverse = kiss_fftr_alloc(N, 1);
  C = static_cast<unsigned int>(chn_alloc[setup].size());

  // Allocate per-channel buffers
  outbuf.resize((N + N / 2) * C);
  signal.resize(C, std::vector<cplx>(N));

  // Init the window function
  for (unsigned int k = 0; k < N; k++)
    wnd[k] = sqrt(0.5 * (1 - cos(2 * pi * k / N)) / N);

  // set default parameters
  lo_cut = 40.0f / sample_rate * 2;
  hi_cut = 90.0f / sample_rate * 2;
}

DPL2FSDecoder::~DPL2FSDecoder()
{
  kiss_fftr_free(forward);
  kiss_fftr_free(inverse);
}

// decode a stereo chunk, produces a multichannel chunk of the same size
// (lagged)
float* DPL2FSDecoder::decode(const float* input)
{
  // append incoming data to the end of the input buffer
  memcpy(&inbuf[N], &input[0], 8 * N);
  // process first and second half, overlapped
  buffered_decode(&inbuf[0]);
  buffered_decode(&inbuf[N]);
  // shift last half of the input to the beginning (for overlapping with a
  // future block)
  memcpy(&inbuf[0], &inbuf[2 * N], 4 * N);

  return &outbuf[0];
}

// flush the internal buffers
void DPL2FSDecoder::flush()
{
  memset(&outbuf[0], 0, outbuf.size() * 4);
  memset(&inbuf[0], 0, inbuf.size() * 4);
}

// helper functions
inline double DPL2FSDecoder::amplitude(const cplx x)
{
  return hypot(x.real(), x.imag());
}
inline double DPL2FSDecoder::phase(const cplx x)
{
  return atan2(x.imag(), x.real());
}
inline cplx DPL2FSDecoder::polar(const double a, const double p)
{
  return cplx(a * cos(p), a * sin(p));
}
inline double DPL2FSDecoder::clamp(const double x)
{
  return std::clamp(x, -1.0, 1.0);
}
// get the distance of the soundfield edge, along a given angle
inline double DPL2FSDecoder::edgedistance(const double a)
{
  return 1.0 / std::max(std::abs(std::cos(a)), std::abs(std::sin(a)));
}
// get the index (and fractional offset!) in a piecewise-linear channel
// allocation grid
int DPL2FSDecoder::map_to_grid(double& x)
{
  const int gp = (x + 1) * 10;
  const int i = std::clamp(gp, 0, 19);
  x = gp - i;
  return i;
}

// decode a block of data and overlap-add it into outbuf

void DPL2FSDecoder::buffered_decode(const float* input)
{
  // demultiplex and apply window function
  for (unsigned int k = 0; k < N; k++)
  {
    lt[k] = wnd[k] * input[k * 2 + 0];
    rt[k] = wnd[k] * input[k * 2 + 1];
  }

  // map into spectral domain
  kiss_fftr(forward, &lt[0], reinterpret_cast<kiss_fft_cpx*>(&lf[0]));
  kiss_fftr(forward, &rt[0], reinterpret_cast<kiss_fft_cpx*>(&rf[0]));

  // compute multichannel output signal in the spectral domain
  for (unsigned int f = 1; f < N / 2; f++)
  {
    // get Lt/Rt amplitudes & phases
    const double ampL = amplitude(lf[f]), ampR = amplitude(rf[f]);
    const double phaseL = phase(lf[f]), phaseR = phase(rf[f]);
    // calculate the amplitude & phase differences
    const double ampDiff = clamp(
        ampL + ampR < std::numeric_limits<double>::epsilon() ? 0 : (ampR - ampL) / (ampR + ampL));
    double phaseDiff = abs(phaseL - phaseR);
    if (phaseDiff > pi)
      phaseDiff = 2 * pi - phaseDiff;

    // decode into x/y soundfield position
    auto [x, y] = transform_decode(ampDiff, phaseDiff);

    // get total signal amplitude
    const double amp_total = hypot(ampL, ampR);
    // and total L/C/R signal phases
    const double phase_of[] = {
        phaseL, atan2(lf[f].imag() + rf[f].imag(), lf[f].real() + rf[f].real()), phaseR};
    // compute 2d channel map indexes p/q and update x/y to fractional offsets
    // in the map grid
    const int p = map_to_grid(x), q = map_to_grid(y);
    // map position to channel volumes
    for (unsigned int c = 0; c < C - 1; c++)
    {
      // look up channel map at respective position (with bilinear
      // interpolation) and build the
      // signal
      std::vector<float*>& a = chn_alloc[setup][c];
      signal[c][f] = polar(amp_total * ((1 - x) * (1 - y) * a[q][p] + x * (1 - y) * a[q][p + 1] +
                                        (1 - x) * y * a[q + 1][p] + x * y * a[q + 1][p + 1]),
                           phase_of[1 + static_cast<int>(copysign(1.0, chn_xsf[setup][c]))]);
    }
  }

  // shift the last 2/3 to the first 2/3 of the output buffer
  memcpy(&outbuf[0], &outbuf[C * N / 2], N * C * 4);
  // and clear the rest
  memset(&outbuf[C * N], 0, C * 4 * N / 2);
  // backtransform each channel and overlap-add
  for (unsigned int c = 0; c < C; c++)
  {
    // back-transform into time domain
    kiss_fftri(inverse, reinterpret_cast<kiss_fft_cpx*>(&signal[c][0]), &dst[0]);
    // add the result to the last 2/3 of the output buffer, windowed (and
    // remultiplex)
    for (unsigned int k = 0; k < N; k++)
      outbuf[C * (k + N / 2) + c] += static_cast<float>(wnd[k] * dst[k]);
  }
}

std::tuple<double, double> DPL2FSDecoder::transform_decode(const double a, const double p)
{
  // x = 1.0047a + 0.46804ap³ - 0.2042ap⁴ + 0.0080586ap⁷ - 0.0001526ap¹⁰ - 0.073512a³p -
  // 0.2499a³p⁴ + 0.016932a³p⁷ - 0.00027707a³p¹⁰ + 0.048105a⁵p⁷ - 0.0065947a⁵p¹⁰ +
  // 0.0016006a⁵p¹¹ - 0.0071132a⁷p⁹ + 0.0022336a⁷p¹¹ - 0.0004804a⁷p¹²

  // y = 0.98592 - 0.62237p + 0.077875p² - 0.0026929p⁴ + 0.4971a²p -
  // 0.00032124a²p⁶ + 9.2491e-6a⁴p¹⁰ + 0.051549a⁹ + 1.0727e-14a¹⁰

  return {clamp(1.0047 * a + 0.46804 * a * std::pow(p, 3) - 0.2042 * a * std::pow(p, 4) +
                0.0080586 * a * std::pow(p, 7) - 0.0001526 * a * std::pow(p, 10) -
                0.073512 * std::pow(a, 3) * p - 0.2499 * std::pow(a, 3) * std::pow(p, 4) +
                0.016932 * std::pow(a, 3) * std::pow(p, 7) -
                0.00027707 * std::pow(a, 3) * std::pow(p, 10) +
                0.048105 * std::pow(a, 5) * std::pow(p, 7) -
                0.0065947 * std::pow(a, 5) * std::pow(p, 10) +
                0.0016006 * std::pow(a, 5) * std::pow(p, 11) -
                0.0071132 * std::pow(a, 7) * std::pow(p, 9) +
                0.0022336 * std::pow(a, 7) * std::pow(p, 11) -
                0.0004804 * std::pow(a, 7) * std::pow(p, 12)),
          clamp(0.98592 - 0.62237 * p + 0.077875 * std::pow(p, 2) - 0.0026929 * std::pow(p, 4) +
                0.4971 * std::pow(a, 2) * p - 0.00032124 * std::pow(a, 2) * std::pow(p, 6) +
                9.2491e-6 * std::pow(a, 4) * std::pow(p, 10) + 0.051549 * std::pow(a, 9) +
                1.0727e-14 * std::pow(a, 10))};
}
