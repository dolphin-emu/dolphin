// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

static u64 DoubleToU64(double d)
{
	union
	{
		double dval;
		u64 ival;
	};
	dval = d;
	return ival;
}

static double U64ToDouble(u64 d)
{
	union
	{
		double dval;
		u64 ival;
	};
	ival = d;
	return dval;
}

static double ForceSingle(double _x)
{
	// convert to float...
	float x = (float)_x;
	if (!cpu_info.bFlushToZero && FPSCR.NI)
	{
		x = FlushToZero(x);
	}
	// ...and back to double:
	return x;
}

static double ForceDouble(double d)
{
	if (!cpu_info.bFlushToZero && FPSCR.NI)
	{
		d = FlushToZero(d);
	}
	return d;
}

u64 AddSinglePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) + U64ToDouble(b)));
}

u64 AddDoublePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) + U64ToDouble(b)));
}

u64 SubSinglePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) - U64ToDouble(b)));
}

u64 SubDoublePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) - U64ToDouble(b)));
}

u64 MultiplySinglePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) * U64ToDouble(b)));
}

u64 MultiplyDoublePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) * U64ToDouble(b)));
}

u64 MaddSinglePrecision(u64 a, u64 b, u64 c)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) * U64ToDouble(b) + U64ToDouble(c)));
}

u64 MaddDoublePrecision(u64 a, u64 b, u64 c)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) * U64ToDouble(b) + U64ToDouble(c)));
}

u64 MsubSinglePrecision(u64 a, u64 b, u64 c)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) * U64ToDouble(b) - U64ToDouble(c)));
}

u64 MsubDoublePrecision(u64 a, u64 b, u64 c)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) * U64ToDouble(b) - U64ToDouble(c)));
}

u64 NegMaddSinglePrecision(u64 a, u64 b, u64 c)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(-(U64ToDouble(a) * U64ToDouble(b) + U64ToDouble(c))));
}

u64 NegMaddDoublePrecision(u64 a, u64 b, u64 c)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(-(U64ToDouble(a) * U64ToDouble(b) + U64ToDouble(c))));
}

u64 NegMsubSinglePrecision(u64 a, u64 b, u64 c)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(-(U64ToDouble(a) * U64ToDouble(b) - U64ToDouble(c))));
}

u64 NegMsubDoublePrecision(u64 a, u64 b, u64 c)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(-(U64ToDouble(a) * U64ToDouble(b) - U64ToDouble(c))));
}

u64 DivSinglePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) / U64ToDouble(b)));
}

u64 DivDoublePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) / U64ToDouble(b)));
}

u64 RoundToSingle(u64 a)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a)));
}