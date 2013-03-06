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
#ifndef FPU_ROUND_MODE_H_
#define FPU_ROUND_MODE_H_
#include "Common.h"

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
	
	void SetSIMDMode(u32 mode);

	/*
   There are two different flavors of float to int conversion:
   _mm_cvtps_epi32() and _mm_cvttps_epi32(). The first rounds
   according to the MXCSR rounding bits. The second one always
   uses round towards zero.
 */
	void SaveSIMDState();
	void LoadSIMDState();
	void LoadDefaultSIMDState();
}
#endif
