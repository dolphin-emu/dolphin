// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"

#define CLAMP			((1 << (BIT_DEPTH - 1)) - 1)
#define MAX_SAMPLES     (1024 * 2) // 64ms = 2048/32000
#define INDEX_MASK      (MAX_SAMPLES * 2 - 1)

class Interpolator {
public:
	virtual u32 interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW) {}
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
	}

	virtual ~Linear();
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
	}

	virtual ~Cubic();
	u32 interpolate(short* samples, unsigned int num, u32& indexR, u32 indexW);
	void setRatio(float ratio);
	void setVolume(s32 lvolume, s32 rvolume);
private:
	float m_ratio;
	float m_lvolume, m_rvolume;
	float m_frac;
};