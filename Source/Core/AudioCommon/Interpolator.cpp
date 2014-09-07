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

void Cubic::populateFloats(u32 start, u32 stop) {
	for (u32 i = start; i < stop; i++)
	{
		float_buffer[i & INDEX_MASK] = twos2float(Common::swap16(mix_buffer[i & INDEX_MASK]));
	}
}

u32 Cubic::interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW) {
	
	unsigned int currentSample = 0;
	errorL1 = 0, errorL2 = 0;
	errorR1 = 0, errorR2 = 0;
	rand1 = 0, rand2 = 0, rand3 = 0, rand4 = 0;
	float templ, tempr;
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
		float f2 = (m_frac * m_frac);
		float f3 = (f2 * m_frac);
		float sampleL, sampleR;
#ifdef _M_SSE
		__declspec(align(16)) float f[4] = { f3, f2, m_frac, 1.0f };
#endif


		float sl0, sl1, sl2, sl3;
		
		/*s0 = twos2float(Common::swap16(mix_buffer[(indexRp) & INDEX_MASK])); //previous
		s1 = twos2float(Common::swap16(mix_buffer[(indexR) & INDEX_MASK])); // current
		s2 = twos2float(Common::swap16(mix_buffer[(indexR2) & INDEX_MASK])); //next
		s3 = twos2float(Common::swap16(mix_buffer[(indexR4) & INDEX_MASK])); //nextnext
		*/
		sl0 = float_buffer[(indexRp) & INDEX_MASK];
		sl1 = float_buffer[(indexR) & INDEX_MASK];
		sl2 = float_buffer[(indexR2) & INDEX_MASK];
		sl3 = float_buffer[(indexR4) & INDEX_MASK];

#ifdef _M_SSE
		GC_ALIGNED16(float) pl[4];

		__m128 c0l = _mm_load_ps(s0_coeff);
		__m128 c1l = _mm_load_ps(s1_coeff);
		__m128 c2l = _mm_load_ps(s2_coeff);
		__m128 c3l = _mm_load_ps(s3_coeff);

		__m128 sl_0 = _mm_load1_ps(&sl0);
		__m128 sl_1 = _mm_load1_ps(&sl1);
		__m128 sl_2 = _mm_load1_ps(&sl2);
		__m128 sl_3 = _mm_load1_ps(&sl3);

		sl_0 = _mm_mul_ps(sl_0, c0l);
		sl_1 = _mm_mul_ps(sl_1, c1l);
		sl_2 = _mm_mul_ps(sl_2, c2l);
		sl_3 = _mm_mul_ps(sl_3, c3l);

		sl_0 = _mm_add_ps(sl_0, sl_1);
		sl_2 = _mm_add_ps(sl_2, sl_3);
		sl_0 = _mm_add_ps(sl_0, sl_2);

		//result is in sl_0
		
		__m128 vl = _mm_load_ps(f);

#if _M_SSE >= 0x401
		__m128 resultl = _mm_dp_ps(sl_0, vl, DPPS_MASK);
		_mm_store_ps(pl, resultl);
#else
		__m128 resultl = _mm_mul_ps(sl_0, vl);
		_mm_store_ps(pl, resultl);
		pl[0] = pl[0] + pl[1] + pl[2] + pl[3];
#endif
		sampleL = pl[0] / 2;
#else
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

		rand2 = rand1;
		rand1 = rand();
		templ = sampleL + DITHER_SHAPE * (errorL1 + errorL1 - errorL2);
		sampleL = templ + DOFFSET + DITHER_SIZE * (float) (rand1 - rand2);

		if (sampleL > 1.0) sampleL = 1.0;
		if (sampleL < -1.0) sampleL = -1.0;
		int sampleLi = float2stwos(sampleL);
		samples[currentSample + 1] = sampleLi;

		errorL2 = errorL1;
		errorL1 = templ - sampleL;

		float sr0, sr1, sr2, sr3;
		/*
		s0 = twos2float(Common::swap16(mix_buffer[(indexRp + 1) & INDEX_MASK])); //previous
		s1 = twos2float(Common::swap16(mix_buffer[(indexR + 1) & INDEX_MASK])); // current
		s2 = twos2float(Common::swap16(mix_buffer[(indexR2 + 1) & INDEX_MASK])); //next
		s3 = twos2float(Common::swap16(mix_buffer[(indexR4 + 1) & INDEX_MASK])); //nextnext
		*/
		sr0 = float_buffer[(indexRp + 1) & INDEX_MASK];
		sr1 = float_buffer[(indexR + 1) & INDEX_MASK];
		sr2 = float_buffer[(indexR2 + 1) & INDEX_MASK];
		sr3 = float_buffer[(indexR4 + 1) & INDEX_MASK];


#ifdef _M_SSE
		GC_ALIGNED16(float) pr[4];

		__m128 c0r = _mm_load_ps(s0_coeff);
		__m128 c1r = _mm_load_ps(s1_coeff);
		__m128 c2r = _mm_load_ps(s2_coeff);
		__m128 c3r = _mm_load_ps(s3_coeff);

		__m128 sr_0 = _mm_load1_ps(&sr0);
		__m128 sr_1 = _mm_load1_ps(&sr1);
		__m128 sr_2 = _mm_load1_ps(&sr2);
		__m128 sr_3 = _mm_load1_ps(&sr3);

		sr_0 = _mm_mul_ps(sr_0, c0r);
		sr_1 = _mm_mul_ps(sr_1, c1r);
		sr_2 = _mm_mul_ps(sr_2, c2r);
		sr_3 = _mm_mul_ps(sr_3, c3r);

		sr_0 = _mm_add_ps(sr_0, sr_1);
		sr_2 = _mm_add_ps(sr_2, sr_3);
		sr_0 = _mm_add_ps(sr_0, sr_2);

		//result is in sr_0

		__m128 vr = _mm_load_ps(f);

#if _M_SSE >= 0x401
		__m128 resultr = _mm_dp_ps(sr_0, vr, DPPS_MASK);
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

		rand4 = rand3;
		rand3 = rand();
		tempr = sampleR + DITHER_SHAPE * (errorR1 + errorR1 - errorR2);
		sampleR = tempr + DOFFSET + DITHER_SIZE * (float) (rand3 - rand4);

		if (sampleR > 1.0) sampleR = 1.0;
		if (sampleR < -1.0) sampleR = -1.0;
		int sampleRi = float2stwos(sampleR);
		samples[currentSample] = sampleRi;

		errorR2 = errorR1;
		errorR1 = tempr - sampleR;

		m_frac += m_ratio;
		indexR += 2 * (int) m_frac;
		float intpart;
		m_frac = modf(m_frac, &intpart);
	}
	return currentSample;
}

// LANCZOS INTERPOLATION
// sinc function * sinc window
float Lanczos::sinc_sinc(float x, float window_width) {
	return (window_width * sin(M_PI * x) * sin(M_PI * x / window_width) / (M_PI * x) * (M_PI * x));
}

void Lanczos::populate_sinc_table() {
	float center = (SINC_SIZE - 1) / 2;
	for (int i = 0; i < SINC_FSIZE; i++) {
		float offset = -1.f * (float)i / (float) SINC_FSIZE;
		for (int j = 0; j < SINC_SIZE; j++) {
			float x = (j + offset - center);
			if (x >= -2 && x <= 2) {
				if (x == 0) {
					m_sinc_table[i][j] = 1;
				}
				else {
					m_sinc_table[i][j] = sinc_sinc(x, center);
				}
			}
		}
	}
}

u32 Lanczos::interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW) {
	u32 currentSample = 0;
	for (; currentSample < num * 2 && ((indexW - indexR) & INDEX_MASK) > 2; currentSample += 2) {
		float* table = m_sinc_table[(int) (m_ratio * SINC_FSIZE)];
		u32 indexR2 = indexR + 2; //next sample

	}
	return currentSample;
}