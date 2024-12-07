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

#include "FreeSurround/ChannelMaps.h"
#include <cmath>

#undef min
#undef max

// FreeSurround implementation
// DPL2FSDecoder::Init() must be called before using the decoder.
DPL2FSDecoder::DPL2FSDecoder() {
  initialized = false;
  buffer_empty = true;
}

DPL2FSDecoder::~DPL2FSDecoder() {
  kiss_fftr_free(forward);
  kiss_fftr_free(inverse);
}

void DPL2FSDecoder::Init(const channel_setup chsetup, const unsigned int blocksize, const unsigned int sample_rate) {
  if (initialized)
    return;

  setup = chsetup;
  N = blocksize;
  samplerate = sample_rate;

  // Initialize the parameters
  wnd = std::vector<double>(N);
  inbuf = std::vector<float>(3 * N);
  lt = std::vector<double>(N);
  rt = std::vector<double>(N);
  dst = std::vector<double>(N);
  lf = std::vector<cplx>(N / 2 + 1);
  rf = std::vector<cplx>(N / 2 + 1);
  forward = kiss_fftr_alloc(N, 0, nullptr, nullptr);
  inverse = kiss_fftr_alloc(N, 1, nullptr, nullptr);
  C = static_cast<unsigned int>(chn_alloc[setup].size());

  // Allocate per-channel buffers
  outbuf.resize((N + N / 2) * C);
  signal.resize(C, std::vector<cplx>(N));

  // Init the window function
  for (unsigned int k = 0; k < N; k++)
    wnd[k] = sqrt(0.5 * (1 - cos(2 * pi * k / N)) / N);

  // set default parameters
  set_circular_wrap(90);
  set_shift(0);
  set_depth(1);
  set_focus(0);
  set_center_image(1);
  set_front_separation(1);
  set_rear_separation(1);
  set_low_cutoff(40.0f / static_cast<float>(samplerate) * 2);
  set_high_cutoff(90.0f / static_cast<float>(samplerate) * 2);
  set_bass_redirection(false);

  initialized = true;
}

// decode a stereo chunk, produces a multichannel chunk of the same size
// (lagged)
float *DPL2FSDecoder::decode(const float *input) {
  if (!initialized)
    return nullptr;

  // append incoming data to the end of the input buffer
  memcpy(&inbuf[N], &input[0], 8 * N);
  // process first and second half, overlapped
  buffered_decode(&inbuf[0]);
  buffered_decode(&inbuf[N]);
  // shift last half of the input to the beginning (for overlapping with a
  // future block)
  memcpy(&inbuf[0], &inbuf[2 * N], 4 * N);
  buffer_empty = false;
  return &outbuf[0];
}

// flush the internal buffers
void DPL2FSDecoder::flush() {
  memset(&outbuf[0], 0, outbuf.size() * 4);
  memset(&inbuf[0], 0, inbuf.size() * 4);
  buffer_empty = true;
}

// number of samples currently held in the buffer
unsigned int DPL2FSDecoder::buffered() const { return buffer_empty ? 0 : N / 2; }

// set soundfield & rendering parameters
void DPL2FSDecoder::set_circular_wrap(const float v) { circular_wrap = v; }
void DPL2FSDecoder::set_shift(const float v) { shift = v; }
void DPL2FSDecoder::set_depth(const float v) { depth = v; }
void DPL2FSDecoder::set_focus(const float v) { focus = v; }
void DPL2FSDecoder::set_center_image(const float v) { center_image = v; }
void DPL2FSDecoder::set_front_separation(const float v) { front_separation = v; }
void DPL2FSDecoder::set_rear_separation(const float v) { rear_separation = v; }
void DPL2FSDecoder::set_low_cutoff(const float v) { lo_cut = v * static_cast<float>(N / 2.0); }
void DPL2FSDecoder::set_high_cutoff(const float v) { hi_cut = v * static_cast<float>(N / 2.0); }
void DPL2FSDecoder::set_bass_redirection(const bool v) { use_lfe = v; }

// helper functions
inline float DPL2FSDecoder::sqr(const double x) { return static_cast<float>(x * x); }

inline double DPL2FSDecoder::amplitude(const cplx &x) { return sqrt(sqr(x.real()) + sqr(x.imag())); }

inline double DPL2FSDecoder::phase(const cplx &x) { return atan2(x.imag(), x.real()); }

inline cplx DPL2FSDecoder::polar(const double a, const double p) { return cplx(a * cos(p), a * sin(p)); }

inline float DPL2FSDecoder::min(const double a, const double b) { return static_cast<float>(a < b ? a : b); }

inline float DPL2FSDecoder::max(const double a, const double b) { return static_cast<float>(a > b ? a : b); }

inline float DPL2FSDecoder::clamp(const double x) { return max(-1, min(1, x)); }

inline float DPL2FSDecoder::sign(const double x) { return static_cast<float>(x < 0 ? -1 : x > 0 ? 1 : 0); }

// get the distance of the soundfield edge, along a given angle
inline double DPL2FSDecoder::edgedistance(const double a) {
  return min(sqrt(1 + sqr(tan(a))), sqrt(1 + sqr(1 / tan(a))));
}

// get the index (and fractional offset!) in a piecewise-linear channel
// allocation grid
int DPL2FSDecoder::map_to_grid(double &x) {
  const double gp = (x + 1) * 0.5 * (grid_res - 1), i = min(grid_res - 2, floor(gp));
  x = gp - i;
  return static_cast<int>(i);
}

// decode a block of data and overlap-add it into outbuf
void DPL2FSDecoder::buffered_decode(const float *input) {
  // demultiplex and apply window function
  for (unsigned int k = 0; k < N; k++) {
    lt[k] = wnd[k] * input[k * 2 + 0];
    rt[k] = wnd[k] * input[k * 2 + 1];
  }

  // map into spectral domain
  kiss_fftr(forward, &lt[0], reinterpret_cast<kiss_fft_cpx *>(&lf[0]));
  kiss_fftr(forward, &rt[0], reinterpret_cast<kiss_fft_cpx *>(&rf[0]));

  // compute multichannel output signal in the spectral domain
  for (unsigned int f = 1; f < N / 2; f++) {
    // get Lt/Rt amplitudes & phases
    const double ampL = amplitude(lf[f]), ampR = amplitude(rf[f]), phaseL = phase(lf[f]), phaseR = phase(rf[f]);
    // calculate the amplitude and phase differences
    const double ampDiff = clamp(ampL + ampR < epsilon ? 0 : (ampR - ampL) / (ampR + ampL));
    double phaseDiff = abs(phaseL - phaseR);
    if (phaseDiff > pi)
      phaseDiff = 2 * pi - phaseDiff;

    // decode into x/y soundfield position
    auto [x, y] = transform_decode(ampDiff, phaseDiff);
    // add wrap control
    transform_circular_wrap(x, y, circular_wrap);
    // add shift control
    y = clamp(y - shift);
    // add depth control
    y = clamp(1 - (1 - y) * depth);
    // add focus control
    transform_focus(x, y, focus);
    // add crossfeed control
    x = clamp(x * (front_separation * (1 + y) / 2 + rear_separation * (1 - y) / 2));

    // get total signal amplitude
    const double amp_total = sqrt(ampL * ampL + ampR * ampR);
    // and total L/C/R signal phases
    const double phase_of[] = {phaseL, atan2(lf[f].imag() + rf[f].imag(), lf[f].real() + rf[f].real()), phaseR};
    // compute 2d channel map indexes p/q and update x/y to fractional offsets
    // in the map grid
    const int p = map_to_grid(x), q = map_to_grid(y);
    // map position to channel volumes
    for (unsigned int c = 0; c < C - 1; c++) {
      // look up the channel map at respective position (with bilinear
      // interpolation) and build the
      // signal
      std::vector<float *> &a = chn_alloc[setup][c];
      signal[c][f] = polar(
        amp_total * ((1 - x) * (1 - y) * a[q][p] +
          x * (1 - y) * a[q][p + 1] +
          (1 - x) * y * a[q + 1][p] +
          x * y * a[q + 1][p + 1]),
        phase_of[1 + static_cast<int>(sign(chn_xsf[setup][c]))]);
    }

    // optionally redirect bass
    if (!use_lfe)
      continue;
    const auto w = static_cast<float>(f);
    if (w >= hi_cut)
      continue;
    // level of LFE channel according to normalized frequency
    double lfe_level = w < lo_cut ? 1 : 0.5 * (1 + cos(pi * (w - lo_cut) / (hi_cut - lo_cut)));
    // assign LFE channel
    signal[C - 1][f] = lfe_level * polar(amp_total, phase_of[1]);
    // subtract the signal from the other channels
    for (unsigned int c = 0; c < C - 1; c++)
      signal[c][f] *= 1 - lfe_level;
  }

  // shift the last 2/3 to the first 2/3 of the output buffer
  memcpy(&outbuf[0], &outbuf[C * N / 2], N * C * 4);
  // and clear the rest
  memset(&outbuf[C * N], 0, C * 4 * N / 2);
  // backtransform each channel and overlap-add
  for (unsigned int c = 0; c < C; c++) {
    // back-transform into time domain
    kiss_fftri(inverse, reinterpret_cast<kiss_fft_cpx *>(&signal[c][0]), &dst[0]);
    // add the result to the last 2/3 of the output buffer, windowed (and
    // remultiplex)
    for (unsigned int k = 0; k < N; k++)
      outbuf[C * (k + N / 2) + c] += static_cast<float>(wnd[k] * dst[k]);
  }
}

// transform amp/phase difference space into x/y soundfield space
std::tuple<double, double> DPL2FSDecoder::transform_decode(const double amp, const double phase) {
  return std::make_tuple(calculate_x(amp, phase), calculate_y(amp, phase));
}

float DPL2FSDecoder::calculate_x(const double amp, const double phase) {
  const double ap3 = amp * pow(phase, 3), ap4 = amp * pow(phase, 4), ap7 = amp * pow(phase, 7),
      ap8 = amp * pow(phase, 8), a3p = pow(amp, 3) * phase, a3p4 = pow(amp, 3) * pow(phase, 4),
      a3p7 = pow(amp, 3) * pow(phase, 7), a3p12 = pow(amp, 3) * pow(phase, 7),
      a5p7 = pow(amp, 5) * pow(phase, 7), a5p12 = pow(amp, 5) * pow(phase, 12),
      a5p15 = pow(amp, 5) * pow(phase, 15), a7p9 = pow(amp, 7) * pow(phase, 9),
      a7p15 = pow(amp, 7) * pow(phase, 15), a8p16 = pow(amp, 8) * pow(phase, 16);

  return clamp(1.0047 * amp + 0.46804 * ap3 - 0.2042 * ap4 + 0.0080586 * ap7 - 0.0001526 * ap8 - 0.073512 * a3p +
               0.2499 * a3p4 - 0.016932 * a3p7 + 0.00027707 * a3p12 + 0.048105 * a5p7 - 0.0065947 * a5p12 +
               0.0016006 * a5p15 - 0.0071132 * a7p9 + 0.0022336 * a7p15 - 0.0004804 * a8p16);
}

float DPL2FSDecoder::calculate_y(const double amp, const double phase) {
  const double p2 = pow(phase, 2), p5 = pow(phase, 5), a2p = pow(amp, 2) * phase, a2p6 = pow(amp, 2) * pow(phase, 6),
      a4p7 = pow(amp, 4) * pow(phase, 7), a8 = pow(amp, 8), a10 = pow(amp, 10);

  return clamp(0.98592 - 0.62237 * phase + 0.077875 * p2 - 0.0026929 * p5 + 0.4971 * a2p - 0.00032124 * a2p6 +
               9.2491e-006 * a4p7 + 0.051549 * a8 + 1.0727e-014 * a10);
}

// apply a circular_wrap transformation to some position
void DPL2FSDecoder::transform_circular_wrap(double &x, double &y, double refangle) {
  if (refangle == 90)
    return;
  refangle = refangle * pi / 180;
  constexpr double baseangle = pi / 2;
  // translate into edge-normalized polar coordinates
  double ang = atan2(x, y), len = sqrt(x * x + y * y);
  len = len / edgedistance(ang);
  // apply circular_wrap transform
  if (abs(ang) < baseangle / 2)
    // angle falls within the front region (to be enlarged)
    ang *= refangle / baseangle;
  else
    // angle falls within the rear region (to be shrunken)
    ang = pi - (-(((refangle - 2 * pi) * (pi - abs(ang)) * sign(ang)) /
                  (2 * pi - baseangle)));
  // translate back into soundfield position
  len = len * edgedistance(ang);
  x = clamp(sin(ang) * len);
  y = clamp(cos(ang) * len);
}

// apply a focus transformation to some position
void DPL2FSDecoder::transform_focus(double &x, double &y, const double focus) {
  if (focus == 0)
    return;
  const double ang = atan2(x, y);
  // translate into edge-normalized polar coordinates
  double len = clamp(sqrt(x * x + y * y) / edgedistance(ang));
  // apply focus
  len = focus > 0 ? 1 - pow(1 - len, 1 + focus * 20) : pow(len, 1 - focus * 20);
  // back-transform into Euclidean soundfield position
  len = len * edgedistance(ang);
  x = clamp(sin(ang) * len);
  y = clamp(cos(ang) * len);
}
