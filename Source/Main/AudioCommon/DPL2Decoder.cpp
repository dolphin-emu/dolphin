// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Dolby Pro Logic 2 decoder from ffdshow-tryout
//  * Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//  * Copyright (c) 2004-2006 Milan Cutka
//  * based on mplayer HRTF plugin by ylai

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <numeric>
#include <vector>

#include "AudioCommon/DPL2Decoder.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

static int olddelay = -1;
static unsigned int oldfreq = 0;
static unsigned int dlbuflen;
static int cyc_pos;
static float l_fwr, r_fwr, lpr_fwr, lmr_fwr;
static std::vector<float> fwrbuf_l, fwrbuf_r;
static float adapt_l_gain, adapt_r_gain, adapt_lpr_gain, adapt_lmr_gain;
static std::vector<float> lf, rf, lr, rr, cf, cr;
static float LFE_buf[256];
static unsigned int lfe_pos;
static std::vector<float> filter_coefs_lfe;
static unsigned int len125;

template <class T>
static float DotProduct(int count, const T* buf, const std::vector<float>& coeffs, int offset)
{
  return std::inner_product(buf, buf + count, coeffs.begin() + offset, T(0));
}

template <class T>
static T FIRFilter(const T* buf, int pos, int len, int count, const std::vector<float>& coeffs)
{
  int count1, count2;

  if (pos >= count)
  {
    pos -= count;
    count1 = count;
    count2 = 0;
  }
  else
  {
    count2 = pos;
    count1 = count - pos;
    pos = len - count1;
  }

  // high part of window
  const T* ptr = &buf[pos];

  float r1 = DotProduct(count1, ptr, coeffs, 0);
  float r2 = DotProduct(count2, buf, coeffs, count1);
  return T(r1 + r2);
}

/*
// Hamming
//                        2*pi*k
// w(k) = 0.54 - 0.46*cos(------), where 0 <= k < N
//                         N-1
//
// n window length
// returns buffer with the window parameters
*/
static std::vector<float> Hamming(int n)
{
  std::vector<float> w(n);

  float k = static_cast<float>(2.0 * M_PI / (n - 1));

  // Calculate window coefficients
  for (int i = 0; i < n; i++)
    w[i] = static_cast<float>(0.54 - 0.46 * cos(k * i));

  return w;
}

// FIR filter design

/* Design FIR filter using the Window method

n     filter length must be odd for HP and BS filters
fc    cutoff frequencies (1 for LP and HP, 2 for BP and BS)
0 < fc < 1 where 1 <=> Fs/2
flags window and filter type as defined in filter.h
variables are ored together: i.e. LP|HAMMING will give a
low pass filter designed using a hamming window
opt   beta constant used only when designing using kaiser windows

returns buffer for the filter taps (will be n long)
*/
static std::vector<float> DesignFIR(unsigned int n, float fc, float opt)
{
  const unsigned int o = n & 1;                 // Indicator for odd filter length
  const unsigned int end = ((n + 1) >> 1) - o;  // Loop end

  // Cutoff frequency must be < 0.5 where 0.5 <=> Fs/2
  const float fc1 = MathUtil::Clamp(fc, 0.001f, 1.0f) / 2;

  const float k1 = 2 * static_cast<float>(M_PI) * fc1;  // Cutoff frequency in rad/s
  const float k2 = 0.5f * static_cast<float>(1 - o);    // Time offset if filter has even length
  float g = 0.0f;                                       // Gain

  // Sanity check
  if (n == 0)
    return {};

  // Get window coefficients
  std::vector<float> w = Hamming(n);

  // Low pass filter

  // If the filter length is odd, there is one point which is exactly
  // in the middle. The value at this point is 2*fCutoff*sin(x)/x,
  // where x is zero. To make sure nothing strange happens, we set this
  // value separately.
  if (o)
  {
    w[end] = fc1 * w[end] * 2.0f;
    g = w[end];
  }

  // Create filter
  for (u32 i = 0; i < end; i++)
  {
    float t1 = static_cast<float>(i + 1) - k2;
    w[end - i - 1] = w[n - end + i] =
        static_cast<float>(w[end - i - 1] * sin(k1 * t1) / (M_PI * t1));  // Sinc
    g += 2 * w[end - i - 1];                                              // Total gain in filter
  }

  // Normalize gain
  g = 1 / g;
  for (u32 i = 0; i < n; i++)
    w[i] *= g;

  return w;
}

static void OnSeek()
{
  l_fwr = r_fwr = lpr_fwr = lmr_fwr = 0;
  std::fill(fwrbuf_l.begin(), fwrbuf_l.end(), 0.0f);
  std::fill(fwrbuf_r.begin(), fwrbuf_r.end(), 0.0f);
  adapt_l_gain = adapt_r_gain = adapt_lpr_gain = adapt_lmr_gain = 0;
  std::fill(lf.begin(), lf.end(), 0.0f);
  std::fill(rf.begin(), rf.end(), 0.0f);
  std::fill(lr.begin(), lr.end(), 0.0f);
  std::fill(rr.begin(), rr.end(), 0.0f);
  std::fill(cf.begin(), cf.end(), 0.0f);
  std::fill(cr.begin(), cr.end(), 0.0f);
  lfe_pos = 0;
  memset(LFE_buf, 0, sizeof(LFE_buf));
}

static void Done()
{
  OnSeek();

  filter_coefs_lfe.clear();
}

static std::vector<float> CalculateCoefficients125HzLowpass(int rate)
{
  len125 = 256;
  float f = 125.0f / (rate / 2);
  std::vector<float> coeffs = DesignFIR(len125, f, 0);
  static const float M3_01DB = 0.7071067812f;
  for (unsigned int i = 0; i < len125; i++)
  {
    coeffs[i] *= M3_01DB;
  }
  return coeffs;
}

static float PassiveLock(float x)
{
  static const float MATAGCLOCK =
      0.2f; /* AGC range (around 1) where the matrix behaves passively */
  const float x1 = x - 1;
  const float ax1s = fabs(x - 1) * (1.0f / MATAGCLOCK);
  return x1 - x1 / (1 + ax1s * ax1s) + 1;
}

static void MatrixDecode(const float* in, const int k, const int il, const int ir, bool decode_rear,
                         const int _dlbuflen, float _l_fwr, float _r_fwr, float _lpr_fwr,
                         float _lmr_fwr, float* _adapt_l_gain, float* _adapt_r_gain,
                         float* _adapt_lpr_gain, float* _adapt_lmr_gain, float* _lf, float* _rf,
                         float* _lr, float* _rr, float* _cf)
{
  static const float M9_03DB = 0.3535533906f;
  static const float MATAGCTRIG = 8.0f;  /* (Fuzzy) AGC trigger */
  static const float MATAGCDECAY = 1.0f; /* AGC baseline decay rate (1/samp.) */
  static const float MATCOMPGAIN =
      0.37f; /* Cross talk compensation gain,  0.50 - 0.55 is full cancellation. */

  const int kr = (k + olddelay) % _dlbuflen;
  float l_gain = (_l_fwr + _r_fwr) / (1 + _l_fwr + _l_fwr);
  float r_gain = (_l_fwr + _r_fwr) / (1 + _r_fwr + _r_fwr);
  // The 2nd axis has strong gain fluctuations, and therefore require
  // limits.  The factor corresponds to the 1 / amplification of (Lt
  // - Rt) when (Lt, Rt) is strongly correlated. (e.g. during
  // dialogues).  It should be bigger than -12 dB to prevent
  // distortion.
  float lmr_lim_fwr = _lmr_fwr > M9_03DB * _lpr_fwr ? _lmr_fwr : M9_03DB * _lpr_fwr;
  float lpr_gain = (_lpr_fwr + lmr_lim_fwr) / (1 + _lpr_fwr + _lpr_fwr);
  float lmr_gain = (_lpr_fwr + lmr_lim_fwr) / (1 + lmr_lim_fwr + lmr_lim_fwr);
  float lmr_unlim_gain = (_lpr_fwr + _lmr_fwr) / (1 + _lmr_fwr + _lmr_fwr);
  float lpr, lmr;
  float l_agc, r_agc, lpr_agc, lmr_agc;
  float f, d_gain, c_gain, c_agc_cfk;

  /*** AXIS NO. 1: (Lt, Rt) -> (C, Ls, Rs) ***/
  /* AGC adaption */
  d_gain = (fabs(l_gain - *_adapt_l_gain) + fabs(r_gain - *_adapt_r_gain)) * 0.5f;
  f = d_gain * (1.0f / MATAGCTRIG);
  f = MATAGCDECAY - MATAGCDECAY / (1 + f * f);
  *_adapt_l_gain = (1 - f) * *_adapt_l_gain + f * l_gain;
  *_adapt_r_gain = (1 - f) * *_adapt_r_gain + f * r_gain;
  /* Matrix */
  l_agc = in[il] * PassiveLock(*_adapt_l_gain);
  r_agc = in[ir] * PassiveLock(*_adapt_r_gain);
  _cf[k] = (l_agc + r_agc) * static_cast<float>(M_SQRT1_2);
  if (decode_rear)
  {
    _lr[kr] = _rr[kr] = (l_agc - r_agc) * static_cast<float>(M_SQRT1_2);
    // Stereo rear channel is steered with the same AGC steering as
    // the decoding matrix. Note this requires a fast updating AGC
    // at the order of 20 ms (which is the case here).
    _lr[kr] *= (_l_fwr + _l_fwr) / (1 + _l_fwr + _r_fwr);
    _rr[kr] *= (_r_fwr + _r_fwr) / (1 + _l_fwr + _r_fwr);
  }

  /*** AXIS NO. 2: (Lt + Rt, Lt - Rt) -> (L, R) ***/
  lpr = (in[il] + in[ir]) * static_cast<float>(M_SQRT1_2);
  lmr = (in[il] - in[ir]) * static_cast<float>(M_SQRT1_2);
  /* AGC adaption */
  d_gain = fabs(lmr_unlim_gain - *_adapt_lmr_gain);
  f = d_gain * (1.0f / MATAGCTRIG);
  f = MATAGCDECAY - MATAGCDECAY / (1 + f * f);
  *_adapt_lpr_gain = (1 - f) * *_adapt_lpr_gain + f * lpr_gain;
  *_adapt_lmr_gain = (1 - f) * *_adapt_lmr_gain + f * lmr_gain;
  /* Matrix */
  lpr_agc = lpr * PassiveLock(*_adapt_lpr_gain);
  lmr_agc = lmr * PassiveLock(*_adapt_lmr_gain);
  _lf[k] = (lpr_agc + lmr_agc) * static_cast<float>(M_SQRT1_2);
  _rf[k] = (lpr_agc - lmr_agc) * static_cast<float>(M_SQRT1_2);

  /*** CENTER FRONT CANCELLATION ***/
  // A heuristic approach exploits that Lt + Rt gain contains the
  // information about Lt, Rt correlation.  This effectively reshapes
  // the front and rear "cones" to concentrate Lt + Rt to C and
  // introduce Lt - Rt in L, R.
  /* 0.67677 is the empirical lower bound for lpr_gain. */
  c_gain = 8 * (*_adapt_lpr_gain - 0.67677f);
  c_gain = c_gain > 0 ? c_gain : 0;
  // c_gain should not be too high, not even reaching full
  // cancellation (~ 0.50 - 0.55 at current AGC implementation), or
  // the center will sound too narrow. */
  c_gain = MATCOMPGAIN / (1 + c_gain * c_gain);
  c_agc_cfk = c_gain * _cf[k];
  _lf[k] -= c_agc_cfk;
  _rf[k] -= c_agc_cfk;
  _cf[k] += c_agc_cfk + c_agc_cfk;
}

void DPL2Decode(float* samples, int numsamples, float* out)
{
  static const unsigned int FWRDURATION = 240;  // FWR average duration (samples)
  static const int cfg_delay = 0;
  static const unsigned int fmt_freq = 48000;
  static const unsigned int fmt_nchannels = 2;  // input channels

  int cur = 0;

  if (olddelay != cfg_delay || oldfreq != fmt_freq)
  {
    Done();
    olddelay = cfg_delay;
    oldfreq = fmt_freq;
    dlbuflen = std::max(FWRDURATION, (fmt_freq * cfg_delay / 1000));  //+(len7000-1);
    cyc_pos = dlbuflen - 1;
    fwrbuf_l.resize(dlbuflen);
    fwrbuf_r.resize(dlbuflen);
    lf.resize(dlbuflen);
    rf.resize(dlbuflen);
    lr.resize(dlbuflen);
    rr.resize(dlbuflen);
    cf.resize(dlbuflen);
    cr.resize(dlbuflen);
    filter_coefs_lfe = CalculateCoefficients125HzLowpass(fmt_freq);
    lfe_pos = 0;
    memset(LFE_buf, 0, sizeof(LFE_buf));
  }

  float* in = samples;                           // Input audio data
  float* end = in + numsamples * fmt_nchannels;  // Loop end

  while (in < end)
  {
    const int k = cyc_pos;

    const int fwr_pos = (k + FWRDURATION) % dlbuflen;
    /* Update the full wave rectified total amplitude */
    /* Input matrix decoder */
    l_fwr += fabs(in[0]) - fabs(fwrbuf_l[fwr_pos]);
    r_fwr += fabs(in[1]) - fabs(fwrbuf_r[fwr_pos]);
    lpr_fwr += fabs(in[0] + in[1]) - fabs(fwrbuf_l[fwr_pos] + fwrbuf_r[fwr_pos]);
    lmr_fwr += fabs(in[0] - in[1]) - fabs(fwrbuf_l[fwr_pos] - fwrbuf_r[fwr_pos]);

    /* Matrix encoded 2 channel sources */
    fwrbuf_l[k] = in[0];
    fwrbuf_r[k] = in[1];
    MatrixDecode(in, k, 0, 1, true, dlbuflen, l_fwr, r_fwr, lpr_fwr, lmr_fwr, &adapt_l_gain,
                 &adapt_r_gain, &adapt_lpr_gain, &adapt_lmr_gain, &lf[0], &rf[0], &lr[0], &rr[0],
                 &cf[0]);

    out[cur + 0] = lf[k];
    out[cur + 1] = rf[k];
    out[cur + 2] = cf[k];
    LFE_buf[lfe_pos] = (lf[k] + rf[k] + 2.0f * cf[k] + lr[k] + rr[k]) / 2.0f;
    out[cur + 3] = FIRFilter(LFE_buf, lfe_pos, len125, len125, filter_coefs_lfe);
    lfe_pos++;
    if (lfe_pos == len125)
    {
      lfe_pos = 0;
    }
    out[cur + 4] = lr[k];
    out[cur + 5] = rr[k];
    // Next sample...
    in += 2;
    cur += 6;
    cyc_pos--;
    if (cyc_pos < 0)
    {
      cyc_pos += dlbuflen;
    }
  }
}

void DPL2Reset()
{
  olddelay = -1;
  oldfreq = 0;
  filter_coefs_lfe.clear();
}
