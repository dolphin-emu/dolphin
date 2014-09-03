// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Interpolator.h"
#include "Common/MathUtil.h"
#include <time.h>

float Interpolator::twos2float(u16 s) {
	int n = (s16) s;
	if (n > 0) {
		return (float) (n / (float) 0x7fff);
	}
	else {
		return (float) (n / (float) 0x8000);
	}
}

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
		MathUtil::Clamp(&sampleL, -32767, 32767);
		samples[currentSample + 1] = sampleL;

		s16 r1 = Common::swap16(mix_buffer[(indexR + 1) & INDEX_MASK]); //current
		s16 r2 = Common::swap16(mix_buffer[(indexR2 + 1) & INDEX_MASK]); //next
		int sampleR = ((r1 << 16) + (r2 - r1) * (u16) m_frac) >> 16;
		sampleR = (sampleR * m_rvolume) >> 8;
		sampleR += samples[currentSample];
		MathUtil::Clamp(&sampleR, -32767, 32767);
		samples[currentSample] = sampleR;

		m_frac += m_ratio;
		indexR += 2 * (u16) (m_frac >> 16);
		m_frac &= 0xffff;
	}
	return currentSample;
}

void Cubic::setRatio(float ratio) {
	m_ratio = ratio;
}

void Cubic::setVolume(s32 lvolume, s32 rvolume) {
	m_lvolume = (float) lvolume / 256.f;
	m_rvolume = (float) lvolume / 256.f;
}

u32 Cubic::interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW) {
	srand((u32) time(NULL));
	unsigned int currentSample = 0;
	int rand1 = 0, rand2 = 0, rand3 = 0, rand4 = 0;
	float errorL1 = 0, errorL2 = 0;
	float errorR1 = 0, errorR2 = 0;
	float shape = 0.5f;
	float w = (0xFFFF);
	float width = 1.f / w;
	float dither = width / RAND_MAX;
	float doffset = width * 0.5f;
	float temp;

	for (; currentSample < num * 2 && ((indexW - indexR) & INDEX_MASK) > 2; currentSample += 2)
	{

		u32 indexRp = indexR - 2; //previous sample
		u32 indexR2 = indexR + 2; //next
		u32 indexR4 = indexR + 4; //nextnext
		float f2 = (m_frac * m_frac);
		float f3 = (f2 * m_frac);

		float s0, s1, s2, s3;
		float a, b, c, d;
		s0 = twos2float(Common::swap16(mix_buffer[(indexRp) & INDEX_MASK])); //previous
		s1 = twos2float(Common::swap16(mix_buffer[(indexR) & INDEX_MASK])); // current
		s2 = twos2float(Common::swap16(mix_buffer[(indexR2) & INDEX_MASK])); //next
		s3 = twos2float(Common::swap16(mix_buffer[(indexR4) & INDEX_MASK])); //nextnext
		a = -s0 + (3 * (s1 - s2)) + s3;
		b = (2 * s0) - (5 * s1) + (4 * s2) - s3;
		c = -s0 + s2;
		d = 2 * s1;
		float sampleL = a * f3;
		sampleL += b * f2;
		sampleL += c * m_frac;
		sampleL += d;
		sampleL /= 2;
		sampleL = sampleL * m_lvolume;
		sampleL += twos2float(samples[currentSample + 1]);

		rand2 = rand1;
		rand1 = rand();
		temp = sampleL + shape * (errorL1 + errorL1 - errorL2);
		sampleL = temp + doffset + dither * (float) (rand1 - rand2);

		if (sampleL > 1.0) sampleL = 1.0;
		if (sampleL < -1.0) sampleL = -1.0;
		int sampleLi = float2stwos(sampleL);
		samples[currentSample + 1] = sampleLi;

		errorL2 = errorL1;
		errorL1 = temp - sampleL;



		s0 = twos2float(Common::swap16(mix_buffer[(indexRp + 1) & INDEX_MASK])); //previous
		s1 = twos2float(Common::swap16(mix_buffer[(indexR + 1) & INDEX_MASK])); // current
		s2 = twos2float(Common::swap16(mix_buffer[(indexR2 + 1) & INDEX_MASK])); //next
		s3 = twos2float(Common::swap16(mix_buffer[(indexR4 + 1) & INDEX_MASK])); //nextnext
		a = -s0 + (3 * (s1 - s2)) + s3;
		b = (2 * s0) - (5 * s1) + (4 * s2) - s3;
		c = -s0 + s2;
		d = 2 * s1;
		float sampleR = a * f3;
		sampleR += b * f2;
		sampleR += c * m_frac;
		sampleR += d;
		sampleR /= 2; // divided by 2
		sampleR = sampleR * m_rvolume;
		sampleR += twos2float(samples[currentSample]);

		rand4 = rand3;
		rand3 = rand();
		temp = sampleR + shape * (errorR1 + errorR1 - errorR2);
		sampleR = temp + doffset + dither * (float) (rand3 - rand4);

		if (sampleR > 1.0) sampleR = 1.0;
		if (sampleR < -1.0) sampleR = -1.0;
		int sampleRi = float2stwos(sampleR);
		samples[currentSample] = sampleRi;

		errorR2 = errorR1;
		errorR1 = temp - sampleR;

		m_frac += m_ratio;
		indexR += 2 * (int) m_frac;
		float intpart;
		m_frac = modf(m_frac, &intpart);
	}
	return currentSample;
}