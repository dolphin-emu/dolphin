// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace FPURoundMode
{
	enum RoundModes
	{
		ROUND_NEAR = 0,
		ROUND_CHOP = 1,
		ROUND_UP   = 2,
		ROUND_DOWN = 3
	};
	enum PrecisionModes {
		PREC_24 = 0,
		PREC_53 = 1,
		PREC_64 = 2
	};
	void SetRoundMode(enum RoundModes mode);

	void SetPrecisionMode(enum PrecisionModes mode);

	void SetSIMDMode(enum RoundModes rounding_mode, bool non_ieee_mode);

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
