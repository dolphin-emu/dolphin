// Copyright 2008 Dolphin Emulator Project
// Copyright 2005 Duddie
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPStacks.h"


// ---------------------------------------------------------------------------------------
// --- SR
// ---------------------------------------------------------------------------------------

static inline void dsp_SR_set_flag(int flag)
{
	g_dsp.r.sr |= flag;
}

static inline bool dsp_SR_is_flag_set(int flag)
{
	return (g_dsp.r.sr & flag) != 0;
}

// ---------------------------------------------------------------------------------------
// --- AR increments, decrements
// ---------------------------------------------------------------------------------------

static inline u16 dsp_increase_addr_reg(u16 reg, s16 _ix)
{
	u32 ar = g_dsp.r.ar[reg];
	u32 wr = g_dsp.r.wr[reg];
	s32 ix = _ix;

	u32 mx = (wr | 1) << 1;
	u32 nar = ar + ix;
	u32 dar = (nar ^ ar ^ ix) & mx;

	if (ix >= 0)
	{
		if (dar > wr) //overflow
			nar -= wr + 1;
	}
	else
	{
		if ((((nar + wr + 1) ^ nar) & dar) <= wr) //underflow or below min for mask
			nar += wr + 1;
	}
	return nar;
}

static inline u16 dsp_decrease_addr_reg(u16 reg, s16 _ix)
{
	u32 ar = g_dsp.r.ar[reg];
	u32 wr = g_dsp.r.wr[reg];
	s32 ix = _ix;

	u32 mx = (wr | 1) << 1;
	u32 nar = ar - ix;
	u32 dar = (nar ^ ar ^ ~ix) & mx;

	if ((u32)ix > 0xFFFF8000) //(ix < 0 && ix != -0x8000)
	{
		if (dar > wr) //overflow
			nar -= wr + 1;
	}
	else
	{
		if ((((nar + wr + 1) ^ nar) & dar) <= wr) //underflow or below min for mask
			nar += wr + 1;
	}
	return nar;
}

static inline u16 dsp_increment_addr_reg(u16 reg)
{
	u32 ar = g_dsp.r.ar[reg];
	u32 wr = g_dsp.r.wr[reg];

	u32 nar = ar + 1;

	if ((nar ^ ar) > ((wr | 1) << 1))
		nar -= wr + 1;
	return nar;
}

static inline u16 dsp_decrement_addr_reg(u16 reg)
{
	u32 ar = g_dsp.r.ar[reg];
	u32 wr = g_dsp.r.wr[reg];

	u32 nar = ar + wr;

	if (((nar ^ ar) & ((wr | 1) << 1)) > wr)
		nar -= wr + 1;
	return nar;
}


// ---------------------------------------------------------------------------------------
// --- reg
// ---------------------------------------------------------------------------------------

static inline u16 dsp_op_read_reg(int _reg)
{
	int reg = _reg & 0x1f;

	switch (reg)
	{
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		return dsp_reg_load_stack(reg - DSP_REG_ST0);
	case DSP_REG_AR0:
	case DSP_REG_AR1:
	case DSP_REG_AR2:
	case DSP_REG_AR3:
		return g_dsp.r.ar[reg - DSP_REG_AR0];
	case DSP_REG_IX0:
	case DSP_REG_IX1:
	case DSP_REG_IX2:
	case DSP_REG_IX3:
		return g_dsp.r.ix[reg - DSP_REG_IX0];
	case DSP_REG_WR0:
	case DSP_REG_WR1:
	case DSP_REG_WR2:
	case DSP_REG_WR3:
		return g_dsp.r.wr[reg - DSP_REG_WR0];
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		return g_dsp.r.ac[reg - DSP_REG_ACH0].h;
	case DSP_REG_CR:     return g_dsp.r.cr;
	case DSP_REG_SR:     return g_dsp.r.sr;
	case DSP_REG_PRODL:  return g_dsp.r.prod.l;
	case DSP_REG_PRODM:  return g_dsp.r.prod.m;
	case DSP_REG_PRODH:  return g_dsp.r.prod.h;
	case DSP_REG_PRODM2: return g_dsp.r.prod.m2;
	case DSP_REG_AXL0:
	case DSP_REG_AXL1:
		return g_dsp.r.ax[reg - DSP_REG_AXL0].l;
	case DSP_REG_AXH0:
	case DSP_REG_AXH1:
		return g_dsp.r.ax[reg - DSP_REG_AXH0].h;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		return g_dsp.r.ac[reg - DSP_REG_ACL0].l;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		return g_dsp.r.ac[reg - DSP_REG_ACM0].m;
	default:
		_assert_msg_(DSP_INT, 0, "cannot happen");
		return 0;
	}
}

static inline void dsp_op_write_reg(int _reg, u16 val)
{
	int reg = _reg & 0x1f;

	switch (reg)
	{
	// 8-bit sign extended registers. Should look at prod.h too...
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		// sign extend from the bottom 8 bits.
		g_dsp.r.ac[reg-DSP_REG_ACH0].h = (u16)(s16)(s8)(u8)val;
		break;

	// Stack registers.
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		dsp_reg_store_stack(reg - DSP_REG_ST0, val);
		break;

	case DSP_REG_AR0:
	case DSP_REG_AR1:
	case DSP_REG_AR2:
	case DSP_REG_AR3:
		g_dsp.r.ar[reg - DSP_REG_AR0] = val;
		break;
	case DSP_REG_IX0:
	case DSP_REG_IX1:
	case DSP_REG_IX2:
	case DSP_REG_IX3:
		g_dsp.r.ix[reg - DSP_REG_IX0] = val;
		break;
	case DSP_REG_WR0:
	case DSP_REG_WR1:
	case DSP_REG_WR2:
	case DSP_REG_WR3:
		g_dsp.r.wr[reg - DSP_REG_WR0] = val;
		break;
	case DSP_REG_CR:     g_dsp.r.cr = val; break;
	case DSP_REG_SR:     g_dsp.r.sr = val; break;
	case DSP_REG_PRODL:  g_dsp.r.prod.l = val; break;
	case DSP_REG_PRODM:  g_dsp.r.prod.m = val; break;
	case DSP_REG_PRODH:  g_dsp.r.prod.h = val; break;
	case DSP_REG_PRODM2: g_dsp.r.prod.m2 = val; break;
	case DSP_REG_AXL0:
	case DSP_REG_AXL1:
		g_dsp.r.ax[reg - DSP_REG_AXL0].l = val;
		break;
	case DSP_REG_AXH0:
	case DSP_REG_AXH1:
		g_dsp.r.ax[reg - DSP_REG_AXH0].h = val;
		break;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		g_dsp.r.ac[reg - DSP_REG_ACL0].l = val;
		break;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		g_dsp.r.ac[reg - DSP_REG_ACM0].m = val;
		break;
	}
}

static inline void dsp_conditional_extend_accum(int reg)
{
	switch (reg)
	{
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		if (g_dsp.r.sr & SR_40_MODE_BIT)
		{
			// Sign extend into whole accum.
			u16 val = g_dsp.r.ac[reg-DSP_REG_ACM0].m;
			g_dsp.r.ac[reg - DSP_REG_ACM0].h = (val & 0x8000) ? 0xFFFF : 0x0000;
			g_dsp.r.ac[reg - DSP_REG_ACM0].l = 0;
		}
	}
}

// ---------------------------------------------------------------------------------------
// --- prod (40-bit)
// ---------------------------------------------------------------------------------------

static inline s64 dsp_get_long_prod()
{
	s64 val   = (s8)(u8)g_dsp.r.prod.h;
	val <<= 32;
	s64 low_prod  = g_dsp.r.prod.m;
	low_prod += g_dsp.r.prod.m2;
	low_prod <<= 16;
	low_prod |= g_dsp.r.prod.l;
	val += low_prod;
	return val;
}

static inline s64 dsp_get_long_prod_round_prodl()
{
	s64 prod = dsp_get_long_prod();

	if (prod & 0x10000)
		prod = (prod + 0x8000) & ~0xffff;
	else
		prod = (prod + 0x7fff) & ~0xffff;

	return prod;
}

// For accurate emulation, this is wrong - but the real prod registers behave
// in completely bizarre ways. Not needed to emulate them correctly for game ucodes.
inline void dsp_set_long_prod(s64 val)
{
	g_dsp.r.prod.val = val & 0x000000FFFFFFFFFFULL;
}

// ---------------------------------------------------------------------------------------
// --- ACC - main accumulators (40-bit)
// ---------------------------------------------------------------------------------------

inline s64 dsp_get_long_acc(int reg)
{
	return ((s64)(g_dsp.r.ac[reg].val << 24) >> 24);
}

inline void dsp_set_long_acc(int _reg, s64 val)
{
	g_dsp.r.ac[_reg].val = (u64)val;
}

inline s64 dsp_convert_long_acc(s64 val) // s64 -> s40
{
	return ((val << 24) >> 24);
}

inline s64 dsp_round_long_acc(s64 val)
{
	if (val & 0x10000)
		val = (val + 0x8000) & ~0xffff;
	else
		val = (val + 0x7fff) & ~0xffff;

	return val;
}

inline s16 dsp_get_acc_l(int _reg)
{
	return (s16)g_dsp.r.ac[_reg].l;
}

inline s16 dsp_get_acc_m(int _reg)
{
	return (s16)g_dsp.r.ac[_reg].m;
}

inline s16 dsp_get_acc_h(int _reg)
{
	return (s16)g_dsp.r.ac[_reg].h;
}

inline u16 dsp_op_read_reg_and_saturate(u8 _reg)
{
	if (g_dsp.r.sr & SR_40_MODE_BIT)
	{
		s64 acc = dsp_get_long_acc(_reg);

		if (acc != (s32)acc)
		{
			if (acc > 0)
				return 0x7fff;
			else
				return 0x8000;
		}
		else
		{
			return g_dsp.r.ac[_reg].m;
		}
	}
	else
	{
		return g_dsp.r.ac[_reg].m;
	}
}

// ---------------------------------------------------------------------------------------
// --- AX - extra accumulators (32-bit)
// ---------------------------------------------------------------------------------------

inline s32 dsp_get_long_acx(int _reg)
{
	return (s32)(((u32)g_dsp.r.ax[_reg].h << 16) | g_dsp.r.ax[_reg].l);
}

inline s16 dsp_get_ax_l(int _reg)
{
	return (s16)g_dsp.r.ax[_reg].l;
}

inline s16 dsp_get_ax_h(int _reg)
{
	return (s16)g_dsp.r.ax[_reg].h;
}
