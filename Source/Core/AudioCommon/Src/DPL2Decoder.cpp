// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Dolby Pro Logic 2 decoder from ffdshow-tryout
//  * Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//  * Copyright (c) 2004-2006 Milan Cutka
//	* based on mplayer HRTF plugin by ylai

#include <functional>
#include <vector>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "DPL2Decoder.h"

#define M_PI 3.14159265358979323846
#define M_SQRT1_2 0.70710678118654752440

int olddelay = -1;
unsigned int oldfreq = 0;
unsigned int dlbuflen;
int cyc_pos;
float l_fwr, r_fwr, lpr_fwr, lmr_fwr;
std::vector<float> fwrbuf_l, fwrbuf_r;
float adapt_l_gain, adapt_r_gain, adapt_lpr_gain, adapt_lmr_gain;
std::vector<float> lf, rf, lr, rr, cf, cr;
float LFE_buf[256];
unsigned int lfe_pos;
float *filter_coefs_lfe;
unsigned int len125;

template<class T,class _ftype_t> static _ftype_t dotproduct(int count,const T *buf,const _ftype_t *coefficients)
{
	float sum0=0,sum1=0,sum2=0,sum3=0;
	for (;count>=4;buf+=4,coefficients+=4,count-=4)
	{
		sum0+=buf[0]*coefficients[0];
		sum1+=buf[1]*coefficients[1];
		sum2+=buf[2]*coefficients[2];
		sum3+=buf[3]*coefficients[3];
	}
	while (count--) sum0+= *buf++ * *coefficients++;
	return sum0+sum1+sum2+sum3;
}

template<class T> static T firfilter(const T *buf, int pos, int len, int count, const float *coefficients)
{
	int count1, count2;

	if (pos >= count)
	{
		pos -= count;
		count1 = count; count2 = 0;
	}
	else
	{
		count2 = pos;
		count1 = count - pos;
		pos = len - count1;
	}

	// high part of window
	const T *ptr = &buf[pos];

	float r1=dotproduct(count1,ptr,coefficients);coefficients+=count1;
	float r2=dotproduct(count2,buf,coefficients);
	return T(r1+r2);
}

template<class T> inline const T& limit(const T& val, const T& min, const T& max)
{
	if (val < min) {
		return min;
	} else if (val > max) {
		return max;
	} else {
		return val;
	}
}

/*
// Hamming
//                        2*pi*k
// w(k) = 0.54 - 0.46*cos(------), where 0 <= k < N
//                         N-1
//
// n window length
// w buffer for the window parameters
*/
void hamming(int n, float* w)
{
	int      i;
	float k = float(2*M_PI/((float)(n-1))); // 2*pi/(N-1)

	// Calculate window coefficients
	for (i=0; i<n; i++)
		*w++ = float(0.54 - 0.46*cos(k*(float)i));
}

/******************************************************************************
*  FIR filter design
******************************************************************************/

/* Design FIR filter using the Window method

n     filter length must be odd for HP and BS filters
w     buffer for the filter taps (must be n long)
fc    cutoff frequencies (1 for LP and HP, 2 for BP and BS)
0 < fc < 1 where 1 <=> Fs/2
flags window and filter type as defined in filter.h
variables are ored together: i.e. LP|HAMMING will give a
low pass filter designed using a hamming window
opt   beta constant used only when designing using kaiser windows

returns 0 if OK, -1 if fail
*/
float* design_fir(unsigned int *n, float* fc, float opt)
{
	unsigned int  o   = *n & 1;            // Indicator for odd filter length
	unsigned int  end = ((*n + 1) >> 1) - o;       // Loop end
	unsigned int  i;                      // Loop index

	float k1 = 2 * float(M_PI);             // 2*pi*fc1
	float k2 = 0.5f * (float)(1 - o);// Constant used if the filter has even length
	float g  = 0.0f;                    // Gain
	float t1;                     // Temporary variables
	float fc1;                      // Cutoff frequencies

	// Sanity check
	if(*n==0) return NULL;
	fc[0]=limit(fc[0],float(0.001),float(1));

	float *w=(float*)calloc(sizeof(float),*n);

	// Get window coefficients
	hamming(*n,w);

	fc1=*fc;
	// Cutoff frequency must be < 0.5 where 0.5 <=> Fs/2
	fc1 = ((fc1 <= 1.0) && (fc1 > 0.0)) ? fc1/2 : 0.25f;
	k1 *= fc1;

	// Low pass filter

	// If the filter length is odd, there is one point which is exactly
	// in the middle. The value at this point is 2*fCutoff*sin(x)/x,
	// where x is zero. To make sure nothing strange happens, we set this
	// value separately.
	if (o)
	{
		w[end] = fc1 * w[end] * 2.0f;
		g=w[end];
	}

	// Create filter
	for (i=0 ; i<end ; i++)
	{
		t1 = (float)(i+1) - k2;
		w[end-i-1] = w[*n-end+i] = float(w[end-i-1] * sin(k1 * t1)/(M_PI * t1)); // Sinc
		g += 2*w[end-i-1]; // Total gain in filter
	}


	// Normalize gain
	g=1/g;
	for (i=0; i<*n; i++)
		w[i] *= g;

	return w;
}

void onSeek(void)
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

void done(void)
{
	onSeek();
	if (filter_coefs_lfe)
	{
		free(filter_coefs_lfe);
	}
	filter_coefs_lfe = NULL;
}

float* calc_coefficients_125Hz_lowpass(int rate)
{
	len125 = 256;
	float f = 125.0f / (rate / 2);
	float *coeffs = design_fir(&len125, &f, 0);
	static const float M3_01DB = 0.7071067812f;
	for (unsigned int i = 0; i < len125; i++)
	{
		coeffs[i] *= M3_01DB;
	}
	return coeffs;
}

float passive_lock(float x)
{
	static const float MATAGCLOCK = 0.2f;  /* AGC range (around 1) where the matrix behaves passively */
	const float x1 = x - 1;
	const float ax1s = fabs(x - 1) * (1.0f / MATAGCLOCK);
	return x1 - x1 / (1 + ax1s * ax1s) + 1;
}

void matrix_decode(const float *in, const int k, const int il,
	const int ir, bool decode_rear,
	const int dlbuflen,
	float l_fwr, float r_fwr,
	float lpr_fwr, float lmr_fwr,
	float *adapt_l_gain, float *adapt_r_gain,
	float *adapt_lpr_gain, float *adapt_lmr_gain,
	float *lf, float *rf, float *lr,
	float *rr, float *cf)
{
	static const float M9_03DB = 0.3535533906f;
	static const float MATAGCTRIG = 8.0f;  /* (Fuzzy) AGC trigger */
	static const float MATAGCDECAY = 1.0f; /* AGC baseline decay rate (1/samp.) */
	static const float MATCOMPGAIN = 0.37f; /* Cross talk compensation gain,  0.50 - 0.55 is full cancellation. */

	const int kr = (k + olddelay) % dlbuflen;
	float l_gain = (l_fwr + r_fwr) / (1 + l_fwr + l_fwr);
	float r_gain = (l_fwr + r_fwr) / (1 + r_fwr + r_fwr);
	/* The 2nd axis has strong gain fluctuations, and therefore require
	limits.  The factor corresponds to the 1 / amplification of (Lt
	- Rt) when (Lt, Rt) is strongly correlated. (e.g. during
	dialogues).  It should be bigger than -12 dB to prevent
	distortion. */
	float lmr_lim_fwr = lmr_fwr > M9_03DB * lpr_fwr ? lmr_fwr : M9_03DB * lpr_fwr;
	float lpr_gain = (lpr_fwr + lmr_lim_fwr) / (1 + lpr_fwr + lpr_fwr);
	float lmr_gain = (lpr_fwr + lmr_lim_fwr) / (1 + lmr_lim_fwr + lmr_lim_fwr);
	float lmr_unlim_gain = (lpr_fwr + lmr_fwr) / (1 + lmr_fwr + lmr_fwr);
	float lpr, lmr;
	float l_agc, r_agc, lpr_agc, lmr_agc;
	float f, d_gain, c_gain, c_agc_cfk;

	/*** AXIS NO. 1: (Lt, Rt) -> (C, Ls, Rs) ***/
	/* AGC adaption */
	d_gain = (fabs(l_gain - *adapt_l_gain) + fabs(r_gain - *adapt_r_gain)) * 0.5f;
	f = d_gain * (1.0f / MATAGCTRIG);
	f = MATAGCDECAY - MATAGCDECAY / (1 + f * f);
	*adapt_l_gain = (1 - f) * *adapt_l_gain + f * l_gain;
	*adapt_r_gain = (1 - f) * *adapt_r_gain + f * r_gain;
	/* Matrix */
	l_agc = in[il] * passive_lock(*adapt_l_gain);
	r_agc = in[ir] * passive_lock(*adapt_r_gain);
	cf[k] = (l_agc + r_agc) * (float)M_SQRT1_2;
	if (decode_rear)
	{
		lr[kr] = rr[kr] = (l_agc - r_agc) * (float)M_SQRT1_2;
		/* Stereo rear channel is steered with the same AGC steering as
		the decoding matrix. Note this requires a fast updating AGC
		at the order of 20 ms (which is the case here). */
		lr[kr] *= (l_fwr + l_fwr) / (1 + l_fwr + r_fwr);
		rr[kr] *= (r_fwr + r_fwr) / (1 + l_fwr + r_fwr);
	}

	/*** AXIS NO. 2: (Lt + Rt, Lt - Rt) -> (L, R) ***/
	lpr = (in[il] + in[ir]) * (float)M_SQRT1_2;
	lmr = (in[il] - in[ir]) * (float)M_SQRT1_2;
	/* AGC adaption */
	d_gain = fabs(lmr_unlim_gain - *adapt_lmr_gain);
	f = d_gain * (1.0f / MATAGCTRIG);
	f = MATAGCDECAY - MATAGCDECAY / (1 + f * f);
	*adapt_lpr_gain = (1 - f) * *adapt_lpr_gain + f * lpr_gain;
	*adapt_lmr_gain = (1 - f) * *adapt_lmr_gain + f * lmr_gain;
	/* Matrix */
	lpr_agc = lpr * passive_lock(*adapt_lpr_gain);
	lmr_agc = lmr * passive_lock(*adapt_lmr_gain);
	lf[k] = (lpr_agc + lmr_agc) * (float)M_SQRT1_2;
	rf[k] = (lpr_agc - lmr_agc) * (float)M_SQRT1_2;

	/*** CENTER FRONT CANCELLATION ***/
	/* A heuristic approach exploits that Lt + Rt gain contains the
	information about Lt, Rt correlation.  This effectively reshapes
	the front and rear "cones" to concentrate Lt + Rt to C and
	introduce Lt - Rt in L, R. */
	/* 0.67677 is the empirical lower bound for lpr_gain. */
	c_gain = 8 * (*adapt_lpr_gain - 0.67677f);
	c_gain = c_gain > 0 ? c_gain : 0;
	/* c_gain should not be too high, not even reaching full
	cancellation (~ 0.50 - 0.55 at current AGC implementation), or
	the center will sound too narrow. */
	c_gain = MATCOMPGAIN / (1 + c_gain * c_gain);
	c_agc_cfk = c_gain * cf[k];
	lf[k] -= c_agc_cfk;
	rf[k] -= c_agc_cfk;
	cf[k] += c_agc_cfk + c_agc_cfk;
}

void dpl2decode(float *samples, int numsamples, float *out)
{
	static const unsigned int FWRDURATION = 240;    /* FWR average duration (samples) */
	static const int cfg_delay = 0;
	static const unsigned int fmt_freq = 48000;
	static const unsigned int fmt_nchannels = 2; // input channels

	int cur = 0;

	if (olddelay != cfg_delay || oldfreq != fmt_freq)
	{
		done();
		olddelay = cfg_delay;
		oldfreq = fmt_freq;
		dlbuflen = std::max(FWRDURATION, (fmt_freq * cfg_delay / 1000)); //+(len7000-1);
		cyc_pos = dlbuflen - 1;
		fwrbuf_l.resize(dlbuflen);
		fwrbuf_r.resize(dlbuflen);
		lf.resize(dlbuflen);
		rf.resize(dlbuflen);
		lr.resize(dlbuflen);
		rr.resize(dlbuflen);
		cf.resize(dlbuflen);
		cr.resize(dlbuflen);
		filter_coefs_lfe = calc_coefficients_125Hz_lowpass(fmt_freq);
		lfe_pos = 0;
		memset(LFE_buf, 0, sizeof(LFE_buf));
	}

	float *in = samples; // Input audio data
	float *end = in + numsamples * fmt_nchannels; // Loop end

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
		matrix_decode(in, k, 0, 1, true, dlbuflen,
			l_fwr, r_fwr,
			lpr_fwr, lmr_fwr,
			&adapt_l_gain, &adapt_r_gain,
			&adapt_lpr_gain, &adapt_lmr_gain,
			&lf[0], &rf[0], &lr[0], &rr[0], &cf[0]);

		out[cur + 0] = lf[k];
		out[cur + 1] = rf[k];
		out[cur + 2] = cf[k];
		LFE_buf[lfe_pos] = (out[0] + out[1]) / 2;
		out[cur + 3] = firfilter(LFE_buf, lfe_pos, len125, len125, filter_coefs_lfe);
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

void dpl2reset()
{
	olddelay = -1;
	oldfreq = 0;
	filter_coefs_lfe = NULL;
}
