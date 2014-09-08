// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Interpolator.h"
#include "Common/MathUtil.h"

#if _M_SSE >= 0x401 && !(defined __GNUC__ && !defined __SSSE3__)
#include "Common/CPUDetect.h"
#include <smmintrin.h>
#elif _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include "Common/CPUDetect.h"
#include <tmmintrin.h>
#endif

// converts u16 (two's complement) to float using asymmetrical conversion
// citing this site: http://blog.bjornroche.com/2009/12/linearity-and-dynamic-range-in-int.html
// as reasons for asymmetrical conversion. Change if you wish.
float Interpolator::twos2float(u16 s) {
	int n = (s16) s;
	if (n > 0) {
		return (float) (n / (float) 0x7fff);
	}
	else {
		return (float) (n / (float) 0x8000);
	}
}

// converts float to s16
s16 Interpolator::float2stwos(float f) {
	int n;
	if (f > 0) {
		n = (s16) (f * 0x7fff);
	}
	else {
		n = (s16) (f * 0x8000);
	}
	return (s16) n;
}

/* LINEAR INTERPOLATION */
void Linear::setRatio(float ratio) {
	m_ratio = (u32)(65536.f * ratio);
}

void Linear::setVolume(s32 lvolume, s32 rvolume) {
	m_lvolume = lvolume;
	m_rvolume = rvolume;
}

// linear using integer math. Moved unchanged from old Mixer.cpp
u32 Linear::interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW) {
	u32 currentSample = 0;
	for (; currentSample < num * 2 && ((indexW - indexR) & INDEX_MASK) > 2; currentSample += 2) {
		u32 indexR2 = indexR + 2; //next sample

		s16 l1 = Common::swap16(mix_buffer[indexR & INDEX_MASK]); //current
		s16 l2 = Common::swap16(mix_buffer[indexR2 & INDEX_MASK]); //next
		int sampleL = ((l1 << 16) + (l2 - l1) * (u16) m_frac) >> 16;
		sampleL = (sampleL * m_lvolume) >> 8;
		sampleL += samples[currentSample + 1];
		MathUtil::Clamp(&sampleL, -CLAMP, CLAMP);
		samples[currentSample + 1] = sampleL;

		s16 r1 = Common::swap16(mix_buffer[(indexR + 1) & INDEX_MASK]); //current
		s16 r2 = Common::swap16(mix_buffer[(indexR2 + 1) & INDEX_MASK]); //next
		int sampleR = ((r1 << 16) + (r2 - r1) * (u16) m_frac) >> 16;
		sampleR = (sampleR * m_rvolume) >> 8;
		sampleR += samples[currentSample];
		MathUtil::Clamp(&sampleR, -CLAMP, CLAMP);
		samples[currentSample] = sampleR;

		m_frac += m_ratio;
		indexR += 2 * (u16) (m_frac >> 16);
		m_frac &= 0xffff;
	}
	return currentSample;
}


/* CUBIC (CATMULL-ROM) INTERPOLATION */
void Cubic::setRatio(float ratio) {
	m_ratio = ratio;
}

void Cubic::setVolume(s32 lvolume, s32 rvolume) {
	m_lvolume = (float) lvolume / 256.f;	// input volumes are from 0 - 256 (see mixer.h)
	m_rvolume = (float) lvolume / 256.f;
}

// avoid calculating conversion to float multiple times for each sample
// called from CMixer::MixerFifo::PushSamples
void Cubic::populateFloats(u32 start, u32 stop) {
	for (u32 i = start; i < stop; i++)
	{
		float_buffer[i & INDEX_MASK] = twos2float(Common::swap16(mix_buffer[i & INDEX_MASK]));
	}
}

/*
	CATMULL-ROM (Cubic)
	out = ((a * t^3) + (b * t^2) + (c * t) + d) / 2
	where
	a = (-1 * s[-1]) + ( 3 * s[0]) + (-3 * s[1]) + ( 1 * s[2]);
	b = ( 2 * s[-1]) + (-5 * s[0]) + ( 4 * s[1]) + (-1 * s[2]);
	c =                (-1 * s[0]) +               ( 1 * s[2]);
	d =							     ( 2 * s[1])
	t = fraction between s[0] and s[1]
*/
u32 Cubic::interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW) {
	
	unsigned int currentSample = 0;
	
	// initialize dithering variables
	m_errorL1 = 0, m_errorL2 = 0;
	m_errorR1 = 0, m_errorR2 = 0;
	m_randL1 = 0, m_randL2 = 0, m_randR1 = 0, m_randR2 = 0;

	float tempL, tempR;
#ifdef _M_SSE
	GC_ALIGNED16(float) s0_coeff[4] = { -1.f, 2.f, -1.f, 0.f };
	GC_ALIGNED16(float) s1_coeff[4] = { 3.f, -5.f, 0.f, 2.f };
	GC_ALIGNED16(float) s2_coeff[4] = { -3.f, 4.f, 1.f, 0.f };
	GC_ALIGNED16(float) s3_coeff[4] = { 1.f, -1.f, 0.f, 0.f };
#endif

	for (; currentSample < num * 2 && ((indexW - indexR) & INDEX_MASK) > 2; currentSample += 2)
	{

		u32 indexRp = indexR - 2; //previous sample
		u32 indexR2 = indexR + 2; //next
		u32 indexR4 = indexR + 4; //nextnext

		// powers of frac
		float f2 = (m_frac * m_frac);
		float f3 = (f2 * m_frac);
		
		float sampleL, sampleR;

#ifdef _M_SSE
		GC_ALIGNED16(float) f[4] = { f3, f2, m_frac, 1.0f };
#endif

		//==== INTERPOLATE LEFT CHANNEL ====
		float sl0 = float_buffer[(indexRp) & INDEX_MASK];
		float sl1 = float_buffer[(indexR) & INDEX_MASK];
		float sl2 = float_buffer[(indexR2) & INDEX_MASK];
		float sl3 = float_buffer[(indexR4) & INDEX_MASK];

#ifdef _M_SSE
		GC_ALIGNED16(float) pl[4];

		__m128 tempL0 = _mm_mul_ps(_mm_load1_ps(&sl0), _mm_load_ps(s0_coeff));
		__m128 tempL1 = _mm_mul_ps(_mm_load1_ps(&sl1), _mm_load_ps(s1_coeff));
		__m128 tempL2 = _mm_mul_ps(_mm_load1_ps(&sl2), _mm_load_ps(s2_coeff));
		__m128 tempL3 = _mm_mul_ps(_mm_load1_ps(&sl3), _mm_load_ps(s3_coeff));

		tempL0 = _mm_add_ps(tempL0, tempL1);
		tempL2 = _mm_add_ps(tempL2, tempL3);
		tempL0 = _mm_add_ps(tempL0, tempL2);
		// tempL0 contains (a, b, c, d) now.

		__m128 vl = _mm_load_ps(f);

#if _M_SSE >= 0x401 // has dpps (dot product)
		__m128 resultl = _mm_dp_ps(tempL0, vl, DPPS_MASK);
		_mm_store_ps(pl, resultl);
#else				// multiply and add instead
		__m128 resultl = _mm_mul_ps(sl_0, vl);
		_mm_store_ps(pl, resultl);
		pl[0] = pl[0] + pl[1] + pl[2] + pl[3];
#endif				// in any case, divide by 2
		sampleL = pl[0] / 2;
#else				// no SSE
		float al, bl, cl, dl;
		al = -sl0 + (3 * (sl1 - sl2)) + sl3;
		bl = (2 * sl0) - (5 * sl1) + (4 * sl2) - sl3;
		cl = -sl0 + sl2;
		dl = 2 * sl1;

		sampleL = al * f3;
		sampleL += bl * f2;
		sampleL += cl * m_frac;
		sampleL += dl;
		sampleL /= 2;
#endif
		
		sampleL = sampleL * m_lvolume;
		sampleL += twos2float(samples[currentSample + 1]);

		// Calculate shaped dithering
		// see http://musicdsp.org/archive.php?classid=5#61
		m_randL2 = m_randL1;
		m_randL1 = rand();
		tempL = sampleL + DITHER_SHAPE * (m_errorL1 + m_errorL1 - m_errorL2);
		sampleL = tempL + DOFFSET + DITHER_SIZE * (float) (m_randL1 - m_randL2);

		// Clamp and add to samples
		// note: even indices are right channel, odd are left
		MathUtil::Clamp(&sampleL, -1.f, 1.f);
		int sampleLi = float2stwos(sampleL);
		samples[currentSample + 1] = sampleLi;

		// update dither error accumulator
		m_errorL2 = m_errorL1;
		m_errorL1 = tempL - sampleL;

		//==== INTERPOLATE RIGHT CHANNEL ====
		// see above comments for description

		float sr0 = float_buffer[(indexRp + 1) & INDEX_MASK];
		float sr1 = float_buffer[(indexR + 1) & INDEX_MASK];
		float sr2 = float_buffer[(indexR2 + 1) & INDEX_MASK];
		float sr3 = float_buffer[(indexR4 + 1) & INDEX_MASK];

#ifdef _M_SSE
		GC_ALIGNED16(float) pr[4];

		__m128 tempR0 = _mm_mul_ps(_mm_load1_ps(&sr0), _mm_load_ps(s0_coeff));
		__m128 tempR1 = _mm_mul_ps(_mm_load1_ps(&sr1), _mm_load_ps(s1_coeff));
		__m128 tempR2 = _mm_mul_ps(_mm_load1_ps(&sr2), _mm_load_ps(s2_coeff));
		__m128 tempR3 = _mm_mul_ps(_mm_load1_ps(&sr3), _mm_load_ps(s3_coeff));

		tempR0 = _mm_add_ps(tempR0, tempR1);
		tempR2 = _mm_add_ps(tempR2, tempR3);
		tempR0 = _mm_add_ps(tempR0, tempR2);
		// tempR0 has (a, b, c, d)

		__m128 vr = _mm_load_ps(f);

#if _M_SSE >= 0x401
		__m128 resultr = _mm_dp_ps(tempR0, vr, DPPS_MASK);
		_mm_store_ps(pr, resultr);
#else
		__m128 resultr = _mm_mul_ps(sr_0, vr);
		_mm_store_ps(pr, resultr);
		pr[0] = pr[0] + pr[1] + pr[2] + pr[3];
#endif
		sampleR = pr[0] / 2;
#else
		float ar, br, cr, dr;
		ar = -sr0 + (3 * (sr1 - sr2)) + sr3;
		br = (2 * sr0) - (5 * sr1) + (4 * sr2) - sr3;
		cr = -sr0 + sr2;
		dr = 2 * sr1;

		sampleR = al * f3;
		sampleR += bl * f2;
		sampleR += cl * m_frac;
		sampleR += dl;
		sampleR /= 2;
#endif

		sampleR = sampleR * m_rvolume;
		sampleR += twos2float(samples[currentSample]);

		m_randR2 = m_randR1;
		m_randR1 = rand();
		tempR = sampleR + DITHER_SHAPE * (m_errorR1 + m_errorR1 - m_errorR2);
		sampleR = tempR + DOFFSET + DITHER_SIZE * (float) (m_randR1 - m_randR2);

		MathUtil::Clamp(&sampleR, -1.f, 1.f);
		int sampleRi = float2stwos(sampleR);
		samples[currentSample] = sampleRi;

		m_errorR2 = m_errorR1;
		m_errorR1 = tempR - sampleR;

		// "increment" the frac.
		m_frac += m_ratio;
		indexR += 2 * (int) m_frac;
		m_frac = m_frac - (int) m_frac;
	}
	return currentSample;
}

// LANCZOS INTERPOLATION
void Lanczos::setRatio(float ratio) {
	m_ratio = ratio;
}

void Lanczos::setVolume(s32 lvolume, s32 rvolume) {
	m_lvolume = (float) lvolume / 256.f;	// input volumes are from 0 - 256 (see mixer.h)
	m_rvolume = (float) lvolume / 256.f;
}

void Lanczos::populateFloats(u32 start, u32 stop) {
	for (u32 i = start; i < stop; i++)
	{
		float_buffer[i & INDEX_MASK] = twos2float(Common::swap16(mix_buffer[i & INDEX_MASK]));
	}
}

/* normalized sinc function * sinc window
									  sin(pi * x)
	normalized sinc = sinc_pi(x) =   -------------
                                        pi * x
									  alpha * sin(pi * x / alpha)
	Lanczos window = sinc window =    ---------------------------
												pi * x
	alpha = window_width
*/
float Lanczos::sinc_sinc(float x, float window_width) {
	return (float)(window_width * sin(M_PI * x) * sin(M_PI * x / window_width) / ((M_PI * x) * (M_PI * x)));
}

/*  Look-up-table for sinc_sinc
	Each row contains points that are all the same offset from integers
	Offset goes from 0 to -1.
	row
	0:	sinc_sinc(-2)		,	sinc_sinc(-1)		, sinc_sinc(0)			, sinc_sinc(1)		, sinc_sinc(2)
	1:	sinc_sinc(-2.000xx)	,	sinc_sinc(-1.000xx) , sinc_sinc(-0.000xx)	, sinc_sinc(0.999xx), sinc_sinc(1.999xx)
	...
FSIZE:	sinc_sinc(-3)		,	sinc_sinc(-2)		, sinc_sinc(-1)			, sinc_sinc(0)		, sinc_sinc(1)

	Since the first column shifts out of the window (of width 2), it is 0 all of the time. There is no need to store or use that first column.
	Hence, the SINC_SIZE is (5 - 1), and the calculation for the "x" below has a +1
*/
void Lanczos::populate_sinc_table() {
	float center = SINC_SIZE / 2;
	for (int i = 0; i < SINC_FSIZE; i++) {
		float offset = -1.f * (float)i / (float) SINC_FSIZE;
		for (int j = 0; j < SINC_SIZE; j++) {
			float x = (j + 1 + offset - center);
			if (x < -2 || x > 2) {
				m_sinc_table[i][j] = 0;
			}
			else if (x == 0) {
				m_sinc_table[i][j] = 1;
			}
			else {
				m_sinc_table[i][j] = sinc_sinc(x, center);
			}
		}
			}
}

u32 Lanczos::interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW) {
	
	// Dither variables initialization
	m_errorL1 = 0, m_errorL2 = 0;
	m_errorR1 = 0, m_errorR2 = 0;
	m_randR1 = 0, m_randR2 = 0, m_randR1 = 0, m_randR2 = 0;
	float templ, tempr;

	u32 currentSample = 0;
	for (; currentSample < num * 2 && ((indexW - indexR) & INDEX_MASK) > 2; currentSample += 2) {
		// get sinc table with *closest* desired offset
		float* table = m_sinc_table[(int) (m_frac * SINC_FSIZE)];

								  // again, don't need sample -2 because it'll always be multiplied by 0
		u32 indexRp = indexR - 2; // sample -1
						 // indexR is sample 0
		u32 indexR2 = indexR + 2; // sample  1
		u32 indexR4 = indexR + 4; // sample  2

		// LEFT CHANNEL
		float sl1 = float_buffer[(indexRp) & INDEX_MASK];
		float sl2 = float_buffer[(indexR) & INDEX_MASK];
		float sl3 = float_buffer[(indexR2) & INDEX_MASK];
		float sl4 = float_buffer[(indexR4) & INDEX_MASK];

		float al = sl1 * table[0];
		float bl = sl2 * table[1];
		float cl = sl3 * table[2];
		float dl = sl4 * table[3];
		float sampleL = al + bl + cl + dl;

		sampleL = sampleL * m_lvolume;
		sampleL += twos2float(samples[currentSample + 1]);

		// dither
		m_randL2 = m_randL1;
		m_randL1 = rand();
		templ = sampleL + DITHER_SHAPE * (m_errorL1 + m_errorL1 - m_errorL2);
		sampleL = templ + DOFFSET + DITHER_SIZE * (float) (m_randL1 - m_randL2);

		// clamp and output
		MathUtil::Clamp(&sampleL, -1.f, 1.f);
		int sampleLi = float2stwos(sampleL);
		samples[currentSample + 1] = sampleLi;

		// update dither accumulators
		m_errorL2 = m_errorL1;
		m_errorL1 = templ - sampleL;

		// RIGHT CHANNEL
		float sr1 = float_buffer[(indexRp + 1) & INDEX_MASK];
		float sr2 = float_buffer[(indexR + 1) & INDEX_MASK];
		float sr3 = float_buffer[(indexR2 + 1) & INDEX_MASK];
		float sr4 = float_buffer[(indexR4 + 1) & INDEX_MASK];

		float ar = sr1 * table[0];
		float br = sr2 * table[1];
		float cr = sr3 * table[2];
		float dr = sr4 * table[3];
		float sampleR = ar + br + cr + dr;

		sampleR = sampleR * m_rvolume;
		sampleR += twos2float(samples[currentSample]);

		m_randR2 = m_randR1;
		m_randR1 = rand();
		tempr = sampleR + DITHER_SHAPE * (m_errorR1 + m_errorR1 - m_errorR2);
		sampleR = tempr + DOFFSET + DITHER_SIZE * (float) (m_randR1 - m_randR2);

		MathUtil::Clamp(&sampleR, -1.f, 1.f);
		int sampleRi = float2stwos(sampleR);
		samples[currentSample] = sampleRi;

		m_errorR2 = m_errorR1;
		m_errorR1 = tempr - sampleR;

		m_frac += m_ratio;
		indexR += 2 * (int) m_frac;
		m_frac = m_frac - (int) m_frac;
	}
	return currentSample;
}