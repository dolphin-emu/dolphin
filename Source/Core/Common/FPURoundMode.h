// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "CommonTypes.h"

namespace FPURoundMode
{
	enum RoundModes
	{
		ROUND_NEAR = 0,
		ROUND_CHOP,
		ROUND_UP,
		ROUND_DOWN
	};
	enum PrecisionModes {
		PREC_24 = 0,
		PREC_53,
		PREC_64
	};
	void SetRoundMode(u32 mode);

	void SetPrecisionMode(u32 mode);

	void SetSIMDMode(u32 roundingMode, u32 nonIEEEMode);

/*
 * There are two different flavors of float to int conversion:
 * _mm_cvtps_epi32() and _mm_cvttps_epi32().
 *
 * The first rounds according to the MXCSR rounding bits.
 * The second one always uses round towards zero.
 */
	void SaveSIMDState();
	void LoadSIMDState();
	void LoadDefaultSIMDState();
}
