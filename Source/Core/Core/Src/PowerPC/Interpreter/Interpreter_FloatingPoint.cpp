// Copyright (C) 2003-2008 Dolphin Project.

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

#include <math.h>

#ifdef _WIN32
#include <intrin.h>
#else
#include <xmmintrin.h>
#endif

#include "../../Core.h"
#include "Interpreter.h"

// If you wanna have fun, read:
// 80007e08 in super monkey ball
// Fun stuff used:
// bso+
// mcrfs (ARGH pulls stuff out of .. FPSCR). it uses this to check the result of frsp mostly
// crclr
// crset
// crxor
// fnabs
// 

// extremely rare
void CInterpreter::Helper_UpdateCR1(double _fValue)
{
	FPSCR.FPRF = 0;
	if (_fValue == 0.0 || _fValue == -0.0)
		FPSCR.FPRF |= 2;
	if (_fValue > 0.0)
		FPSCR.FPRF |= 4;
	if (_fValue < 0.0)
		FPSCR.FPRF |= 8;
	SetCRField(1, (FPSCR.Hex & 0x0000F000) >> 12);
}

bool CInterpreter::IsNAN(double _dValue) 
{ 
	// not implemented
	return _dValue != _dValue; 
}

void CInterpreter::faddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) + rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fdivsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) / rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));  
}

void CInterpreter::fmaddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) =
		static_cast<float>((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fmulsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) * rPS0(_inst.FC));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));  
}

void CInterpreter::fnmaddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(-((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB)));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fnmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(-((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB)));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fresx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(1.0f / rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fsqrtsx(UGeckoInstruction _inst)
{
	static bool bFirst = true;
	if (bFirst)
		PanicAlert("fsqrtsx - Instruction unimplemented");
	bFirst = false;
}

void CInterpreter::fsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) - rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

//
//--- END OF SINGLE PRECISION ---
//

void CInterpreter::fabsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = fabs(rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fcmpo(UGeckoInstruction _inst)
{
	double fa =	rPS0(_inst.FA);
	double fb =	rPS0(_inst.FB);
	u32 compareResult;

	if (IsNAN(fa) || IsNAN(fb)) compareResult = 1; 
	else if(fa < fb)            compareResult = 8;	
	else if(fa > fb)            compareResult = 4;
	else                        compareResult = 2;

	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);

/* missing part
	if ((frA) is an SNaN or (frB) is an SNaN )
		then VXSNAN ¬ 1
		if VE = 0
			then VXVC ¬ 1
		else if ((frA) is a QNaN or (frB) is a QNaN )
		then VXVC ¬ 1 */
}

void CInterpreter::fcmpu(UGeckoInstruction _inst)
{
	double fa =	rPS0(_inst.FA);
	double fb =	rPS0(_inst.FB);

	u32 compareResult;
	if(IsNAN(fa) ||	IsNAN(fb))  compareResult = 1; 
	else if(fa < fb)            compareResult =	8; 
	else if(fa > fb)            compareResult =	4; 
	else                        compareResult = 2;

	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);

/* missing part
	if ((frA) is an SNaN or (frB) is an SNaN)
		then VXSNAN ¬ 1 */
}


// Apply current rounding mode
void CInterpreter::fctiwx(UGeckoInstruction _inst)
{
	double b = rPS0(_inst.FB);
	u32 value;
	if (b > (double)0x7fffffff) 
		value = 0x7fffffff;
	else if (b < -(double)0x7fffffff) 
		value = 0x80000000; 
	else 
		value = (u32)(s32)_mm_cvtsd_si32(_mm_set_sd(b)); // TODO(ector): enforce chop

	riPS0(_inst.FD) = (u64)value; // zero extend

	/* TODO(ector):
	FPSCR[FR] is set if the result is incremented when rounded. 
	FPSCR[FI] is set if the result is inexact.
	*/
	if (_inst.Rc) 
		Helper_UpdateCR1(rPS0(_inst.FD));
}

/*
In the float -> int direction, floating point input values larger than the largest 
representable int result in 0x80000000 (a very negative number) rather than the 
largest representable int on PowerPC. */

// Always round toward zero
void CInterpreter::fctiwzx(UGeckoInstruction _inst)
{
	double b = rPS0(_inst.FB);
	u32 value;
	if (b > (double)0x7fffffff)			
		value = 0x7fffffff;
	else if (b < -(double)0x7fffffff)	
		value = 0x80000000; 
	else								
		value = (u32)(s32)_mm_cvttsd_si32(_mm_set_sd(b)); //TODO(ector): force round toward zero

	riPS0(_inst.FD) = (u64)value;
	if (_inst.Rc) 
		Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fmrx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB);
//	rPS1(_inst.FD) = rPS0(_inst.FD);    // TODO: Should this be here?

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fnabsx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fnegx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
// TODO(ector): TODO(fires): does this apply to all of the below opcodes?
void CInterpreter::frspx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::faddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) + rPS0(_inst.FB);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fdivx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) / rPS0(_inst.FB);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fmaddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fmulx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) * rPS0(_inst.FC);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fnmaddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fnmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::frsqrtex(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = 1.0 / (sqrt(rPS0(_inst.FB)));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fselx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) >= 0.0) ? rPS0(_inst.FC) : rPS0(_inst.FB);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fsqrtx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD)  = sqrt(rPS0(_inst.FB));

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) - rPS0(_inst.FB);

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
