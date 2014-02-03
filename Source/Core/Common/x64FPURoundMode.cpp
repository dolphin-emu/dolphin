// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "FPURoundMode.h"
#include "CPUDetect.h"

#ifndef _WIN32
static const unsigned short FPU_ROUND_NEAR = 0 << 10;
static const unsigned short FPU_ROUND_DOWN = 1 << 10;
static const unsigned short FPU_ROUND_UP   = 2 << 10;
static const unsigned short FPU_ROUND_CHOP = 3 << 10;
static const unsigned short FPU_ROUND_MASK = 3 << 10;
#include <xmmintrin.h>
#endif

// OR-mask for disabling FPU exceptions (bits 7-12 in the MXCSR register)
static const u32 EXCEPTION_MASK = 0x1F80;
// Denormals-Are-Zero (non-IEEE mode: denormal inputs are set to +/- 0)
static const u32 DAZ = 0x40;
// Flush-To-Zero (non-IEEE mode: denormal outputs are set to +/- 0)
static const u32 FTZ = 0x8000;

namespace FPURoundMode
{
	// Get the default SSE states here.
	static u32 saved_sse_state = _mm_getcsr();
	static const u32 default_sse_state = _mm_getcsr();

	void SetRoundMode(u32 mode)
	{
		// Set FPU rounding mode to mimic the PowerPC's
		#ifdef _M_IX86
			// This shouldn't really be needed anymore since we use SSE
		#ifdef _WIN32
			const int table[4] =
			{
				_RC_NEAR,
				_RC_CHOP,
				_RC_UP,
				_RC_DOWN
			};
			_set_controlfp(_MCW_RC, table[mode]);
		#else
			const unsigned short table[4] =
			{
				FPU_ROUND_NEAR,
				FPU_ROUND_CHOP,
				FPU_ROUND_UP,
				FPU_ROUND_DOWN
			};
			unsigned short _mode;
			asm ("fstcw %0" : "=m" (_mode) : );
			_mode = (_mode & ~FPU_ROUND_MASK) | table[mode];
			asm ("fldcw %0" : : "m" (_mode));
		#endif
		#endif
	}

	void SetPrecisionMode(u32 mode)
	{
		#ifdef _M_IX86
			// sets the floating-point lib to 53-bit
			// PowerPC has a 53bit floating pipeline only
			// eg: sscanf is very sensitive
		#ifdef _WIN32
			_control87(_PC_53, MCW_PC);
		#else
			const unsigned short table[4] = {
				0 << 8, // FPU_PREC_24
				2 << 8, // FPU_PREC_53
				3 << 8, // FPU_PREC_64
				3 << 8, // FPU_PREC_MASK
			};
			unsigned short _mode;
			asm ("fstcw %0" : "=m" (_mode));
			_mode = (_mode & ~table[3]) | table[mode];
			asm ("fldcw %0" : : "m" (_mode));
		#endif
		#else
			//x64 doesn't need this - fpu is done with SSE
			//but still - set any useful sse options here
		#endif
	}

	void SetSIMDMode(u32 roundingMode, u32 nonIEEEMode)
	{
		// lookup table for FPSCR.RN-to-MXCSR.RC translation
		static const u32 roundingModeLUT[4] =
		{
			(0 << 13) | EXCEPTION_MASK, // nearest
			(3 << 13) | EXCEPTION_MASK, // -inf
			(2 << 13) | EXCEPTION_MASK, // +inf
			(1 << 13) | EXCEPTION_MASK, // zero
		};
		u32 csr = roundingModeLUT[roundingMode];

		static const u32 denormalLUT[2] =
		{
			FTZ,       // flush-to-zero only
			FTZ | DAZ, // flush-to-zero and denormals-are-zero (may not be supported)
		};
		// FIXME: proper (?) non-IEEE mode emulation causes issues in lots of games
		if (nonIEEEMode && false)
		{
			csr |= denormalLUT[cpu_info.bFlushToZero];
		}
		_mm_setcsr(csr);
	}

	void SaveSIMDState()
	{
		saved_sse_state = _mm_getcsr();
	}
	void LoadSIMDState()
	{
		_mm_setcsr(saved_sse_state);
	}
	void LoadDefaultSIMDState()
	{
		_mm_setcsr(default_sse_state);
	}
}
