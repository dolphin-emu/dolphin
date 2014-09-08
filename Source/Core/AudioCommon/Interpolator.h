// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include <time.h>
#include <cmath>

#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif

#define BIT_DEPTH		16
#define CLAMP			((1 << (BIT_DEPTH - 1)) - 1)
#define MAX_SAMPLES     (1024 * 2) // 64ms = 2048/32000
#define INDEX_MASK      (MAX_SAMPLES * 2 - 1)	// mask for the buffer
#define DPPS_MASK		0xF1	   // tells DPPS to apply to all inputs, and store in 1st index

// Dither defines
#define DITHER_SHAPE	0.5f
#define WORD			(0xFFFF)
#define WIDTH			1.f / WORD
#define DITHER_SIZE		WIDTH / RAND_MAX
#define DOFFSET			WIDTH * DITHER_SHAPE

#define SINC_FSIZE		65536	// sinc table granularity = 1 / SINC_FSIZE.
#define SINC_SIZE		(5 - 1) // see comment for populate_sinc_table()

class Interpolator {
public:
	virtual u32 interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW) { return 0; }
	virtual ~Interpolator() {}
	virtual void setRatio(float ratio) {}
	virtual void setVolume(s32 lvolume, s32 rvolume) {}
	float twos2float(u16 s);
	s16 float2stwos(float f);
protected:
	short* mix_buffer;
};

class Linear : public Interpolator {
public:
	Linear(short* buff_pointer)
		: m_ratio(0)
		, m_lvolume(256)
		, m_rvolume(256)
		, m_frac(0)
	{
		mix_buffer = buff_pointer;
		NOTICE_LOG(AUDIO, "USING LINEAR INTERPOLATOR");
	}

	virtual ~Linear() {}
	u32 interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW);
	void setRatio(float ratio);
	void setVolume(s32 lvolume, s32 rvolume);
private:
	u32 m_ratio;
	s32 m_lvolume, m_rvolume;
	u32 m_frac;
};

class Cubic : public Interpolator {
public:
	Cubic(short* buff_pointer)
		: m_ratio(0)
		, m_lvolume(1.f)
		, m_rvolume(1.f)
		, m_frac(0)
	{
		mix_buffer = buff_pointer;
		memset(float_buffer, 0, sizeof(float_buffer));
		NOTICE_LOG(AUDIO, "USING CUBIC INTERPOLATOR");
		srand((u32) time(NULL));
	}
	virtual ~Cubic() {}
	u32 interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW);
	void setRatio(float ratio);
	void setVolume(s32 lvolume, s32 rvolume);
	void populateFloats(u32 start, u32 stop);
private:
	float float_buffer[MAX_SAMPLES * 2];
	float m_ratio;
	float m_lvolume, m_rvolume;
	float m_frac;
	// dithering vars
	int m_randL1, m_randL2, m_randR1, m_randR2;
	float m_errorL1, m_errorL2;
	float m_errorR1, m_errorR2;
};

class Lanczos : public Interpolator {
public:
	Lanczos(short* buff_pointer)
		: m_ratio(0)
		, m_lvolume(1.f)
		, m_rvolume(1.f)
		, m_frac(0)
	{
		mix_buffer = buff_pointer;
		NOTICE_LOG(AUDIO, "USING LANCZOS INTERPOLATOR");
		srand((u32) time(NULL));
		memset(float_buffer, 0, sizeof(float_buffer));
		memset(m_sinc_table, 0, sizeof(m_sinc_table));
		populate_sinc_table();
	}
	virtual ~Lanczos() {}
	u32 interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW);
	void setRatio(float ratio);
	void setVolume(s32 lvolume, s32 rvolume);
	void populateFloats(u32 start, u32 stop);
private:
	float sinc_sinc(float x, float window_width);
	void populate_sinc_table();
	float float_buffer[MAX_SAMPLES * 2];
	float m_sinc_table[SINC_FSIZE][SINC_SIZE];
	float m_ratio;
	float m_lvolume, m_rvolume;
	float m_frac;
	// dithering vars
	int m_randL1, m_randL2, m_randR1, m_randR2;
	float m_errorL1, m_errorL2;
	float m_errorR1, m_errorR2;
};