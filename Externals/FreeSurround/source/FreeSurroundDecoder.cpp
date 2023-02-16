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

#include "FreeSurround/FreeSurroundDecoder.h"
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

void DPL2FSDecoder::Init(channel_setup chsetup, unsigned int blsize,
                         unsigned int sample_rate) {
  if (!initialized) {
    setup = chsetup;
    N = blsize;
    samplerate = sample_rate;

    // Initialize the parameters
    wnd = std::vector<double>(N);
    inbuf = std::vector<float>(3 * N);
    lt = std::vector<double>(N);
    rt = std::vector<double>(N);
    dst = std::vector<double>(N);
    lf = std::vector<cplx>(N / 2 + 1);
    rf = std::vector<cplx>(N / 2 + 1);
    forward = kiss_fftr_alloc(N, 0, 0, 0);
    inverse = kiss_fftr_alloc(N, 1, 0, 0);
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
    set_low_cutoff(40.0f / samplerate * 2);
    set_high_cutoff(90.0f / samplerate * 2);
    set_bass_redirection(false);

    initialized = true;
  }
}

// decode a stereo chunk, produces a multichannel chunk of the same size
// (lagged)
float *DPL2FSDecoder::decode(float *input) {
  if (initialized) {
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
  return 0;
}

// flush the internal buffers
void DPL2FSDecoder::flush() {
  memset(&outbuf[0], 0, outbuf.size() * 4);
  memset(&inbuf[0], 0, inbuf.size() * 4);
  buffer_empty = true;
}

// number of samples currently held in the buffer
unsigned int DPL2FSDecoder::buffered() { return buffer_empty ? 0 : N / 2; }

// set soundfield & rendering parameters
void DPL2FSDecoder::set_circular_wrap(float v) { circular_wrap = v; }
void DPL2FSDecoder::set_shift(float v) { shift = v; }
void DPL2FSDecoder::set_depth(float v) { depth = v; }
void DPL2FSDecoder::set_focus(float v) { focus = v; }
void DPL2FSDecoder::set_center_image(float v) { center_image = v; }
void DPL2FSDecoder::set_front_separation(float v) { front_separation = v; }
void DPL2FSDecoder::set_rear_separation(float v) { rear_separation = v; }
void DPL2FSDecoder::set_low_cutoff(float v) { lo_cut = v * (N / 2); }
void DPL2FSDecoder::set_high_cutoff(float v) { hi_cut = v * (N / 2); }
void DPL2FSDecoder::set_bass_redirection(bool v) { use_lfe = v; }

// helper functions
inline float DPL2FSDecoder::sqr(double x) { return static_cast<float>(x * x); }
inline double DPL2FSDecoder::amplitude(const cplx &x) {
  return sqrt(sqr(x.real()) + sqr(x.imag()));
}
inline double DPL2FSDecoder::phase(const cplx &x) {
  return atan2(x.imag(), x.real());
}
inline cplx DPL2FSDecoder::polar(double a, double p) {
  return cplx(a * cos(p), a * sin(p));
}
inline float DPL2FSDecoder::min(double a, double b) {
  return static_cast<float>(a < b ? a : b);
}
inline float DPL2FSDecoder::max(double a, double b) {
  return static_cast<float>(a > b ? a : b);
}
inline float DPL2FSDecoder::clamp(double x) { return max(-1, min(1, x)); }
inline float DPL2FSDecoder::sign(double x) {
  return static_cast<float>(x < 0 ? -1 : (x > 0 ? 1 : 0));
}
// get the distance of the soundfield edge, along a given angle
inline double DPL2FSDecoder::edgedistance(double a) {
  return min(sqrt(1 + sqr(tan(a))), sqrt(1 + sqr(1 / tan(a))));
}
// get the index (and fractional offset!) in a piecewise-linear channel
// allocation grid
int DPL2FSDecoder::map_to_grid(double &x) {
  double gp = ((x + 1) * 0.5) * (grid_res - 1),
         i = min(grid_res - 2, floor(gp));
  x = gp - i;
  return static_cast<int>(i);
}

// decode a block of data and overlap-add it into outbuf
void DPL2FSDecoder::buffered_decode(float *input) {
  // demultiplex and apply window function
  for (unsigned int k = 0; k < N; k++) {
    lt[k] = wnd[k] * input[k * 2 + 0];
    rt[k] = wnd[k] * input[k * 2 + 1];
  }

  // map into spectral domain
  kiss_fftr(forward, &lt[0], (kiss_fft_cpx *)&lf[0]);
  kiss_fftr(forward, &rt[0], (kiss_fft_cpx *)&rf[0]);

  // compute multichannel output signal in the spectral domain
  for (unsigned int f = 1; f < N / 2; f++) {
    // get Lt/Rt amplitudes & phases
    double ampL = amplitude(lf[f]), ampR = amplitude(rf[f]);
    double phaseL = phase(lf[f]), phaseR = phase(rf[f]);
    // calculate the amplitude & phase differences
    double ampDiff =
        clamp((ampL + ampR < epsilon) ? 0 : (ampR - ampL) / (ampR + ampL));
    double phaseDiff = abs(phaseL - phaseR);
    if (phaseDiff > pi)
      phaseDiff = 2 * pi - phaseDiff;

    // decode into x/y soundfield position
    double x, y;
    transform_decode(ampDiff, phaseDiff, x, y);
    // add wrap control
    transform_circular_wrap(x, y, circular_wrap);
    // add shift control
    y = clamp(y - shift);
    // add depth control
    y = clamp(1 - (1 - y) * depth);
    // add focus control
    transform_focus(x, y, focus);
    // add crossfeed control
    x = clamp(x *
              (front_separation * (1 + y) / 2 + rear_separation * (1 - y) / 2));

    // get total signal amplitude
    double amp_total = sqrt(ampL * ampL + ampR * ampR);
    // and total L/C/R signal phases
    double phase_of[] = {
        phaseL, atan2(lf[f].imag() + rf[f].imag(), lf[f].real() + rf[f].real()),
        phaseR};
    // compute 2d channel map indexes p/q and update x/y to fractional offsets
    // in the map grid
    int p = map_to_grid(x), q = map_to_grid(y);
    // map position to channel volumes
    for (unsigned int c = 0; c < C - 1; c++) {
      // look up channel map at respective position (with bilinear
      // interpolation) and build the
      // signal
      std::vector<float *> &a = chn_alloc[setup][c];
      signal[c][f] = polar(
          amp_total * ((1 - x) * (1 - y) * a[q][p] + x * (1 - y) * a[q][p + 1] +
                       (1 - x) * y * a[q + 1][p] + x * y * a[q + 1][p + 1]),
          phase_of[1 + static_cast<int>(sign(chn_xsf[setup][c]))]);
    }

    // optionally redirect bass
    if (use_lfe && f < hi_cut) {
      // level of LFE channel according to normalized frequency
      double lfe_level =
          f < lo_cut ? 1
                     : 0.5 * (1 + cos(pi * (f - lo_cut) / (hi_cut - lo_cut)));
      // assign LFE channel
      signal[C - 1][f] = lfe_level * polar(amp_total, phase_of[1]);
      // subtract the signal from the other channels
      for (unsigned int c = 0; c < C - 1; c++)
        signal[c][f] *= (1 - lfe_level);
    }
  }

  // shift the last 2/3 to the first 2/3 of the output buffer
  memcpy(&outbuf[0], &outbuf[C * N / 2], N * C * 4);
  // and clear the rest
  memset(&outbuf[C * N], 0, C * 4 * N / 2);
  // backtransform each channel and overlap-add
  for (unsigned int c = 0; c < C; c++) {
    // back-transform into time domain
    kiss_fftri(inverse, (kiss_fft_cpx *)&signal[c][0], &dst[0]);
    // add the result to the last 2/3 of the output buffer, windowed (and
    // remultiplex)
    for (unsigned int k = 0; k < N; k++)
      outbuf[C * (k + N / 2) + c] += static_cast<float>(wnd[k] * dst[k]);
  }
}

// transform amp/phase difference space into x/y soundfield space
void DPL2FSDecoder::transform_decode(double a, double p, double &x, double &y) {
  x = clamp(1.0047 * a + 0.46804 * a * p * p * p - 0.2042 * a * p * p * p * p +
            0.0080586 * a * p * p * p * p * p * p * p -
            0.0001526 * a * p * p * p * p * p * p * p * p * p * p -
            0.073512 * a * a * a * p - 0.2499 * a * a * a * p * p * p * p +
            0.016932 * a * a * a * p * p * p * p * p * p * p -
            0.00027707 * a * a * a * p * p * p * p * p * p * p * p * p * p +
            0.048105 * a * a * a * a * a * p * p * p * p * p * p * p -
            0.0065947 * a * a * a * a * a * p * p * p * p * p * p * p * p * p *
                p +
            0.0016006 * a * a * a * a * a * p * p * p * p * p * p * p * p * p *
                p * p -
            0.0071132 * a * a * a * a * a * a * a * p * p * p * p * p * p * p *
                p * p +
            0.0022336 * a * a * a * a * a * a * a * p * p * p * p * p * p * p *
                p * p * p * p -
            0.0004804 * a * a * a * a * a * a * a * p * p * p * p * p * p * p *
                p * p * p * p * p);
  y = clamp(
      0.98592 - 0.62237 * p + 0.077875 * p * p - 0.0026929 * p * p * p * p * p +
      0.4971 * a * a * p - 0.00032124 * a * a * p * p * p * p * p * p +
      9.2491e-006 * a * a * a * a * p * p * p * p * p * p * p * p * p * p +
      0.051549 * a * a * a * a * a * a * a * a +
      1.0727e-014 * a * a * a * a * a * a * a * a * a * a);
}

// apply a circular_wrap transformation to some position
void DPL2FSDecoder::transform_circular_wrap(double &x, double &y,
                                            double refangle) {
  if (refangle == 90)
    return;
  refangle = refangle * pi / 180;
  double baseangle = 90 * pi / 180;
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
void DPL2FSDecoder::transform_focus(double &x, double &y, double focus) {
  if (focus == 0)
    return;
  // translate into edge-normalized polar coordinates
  double ang = atan2(x, y),
         len = clamp(sqrt(x * x + y * y) / edgedistance(ang));
  // apply focus
  len = focus > 0 ? 1 - pow(1 - len, 1 + focus * 20) : pow(len, 1 - focus * 20);
  // back-transform into euclidian soundfield position
  len = len * edgedistance(ang);
  x = clamp(sin(ang) * len);
  y = clamp(cos(ang) * len);
}
