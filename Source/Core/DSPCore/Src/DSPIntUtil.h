/*====================================================================

   filename:     gdsp_opcodes_helper.h
   project:      GameCube DSP Tool (gcdsp)
   created:      2005.03.04
   mail:		  duddie@walla.com

   Copyright (c) 2005 Duddie

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   ====================================================================*/

#ifndef _DSP_INT_UTIL_H
#define _DSP_INT_UTIL_H

#include "Common.h"

#include "DSPInterpreter.h"
#include "DSPCore.h"
#include "DSPMemoryMap.h"
#include "DSPStacks.h"


// ---------------------------------------------------------------------------------------
// --- SR
// ---------------------------------------------------------------------------------------
inline void dsp_SR_set_flag(int flag)
{
	g_dsp.r[DSP_REG_SR] |= flag;
}

inline bool dsp_SR_is_flag_set(int flag)
{
	return (g_dsp.r[DSP_REG_SR] & flag) != 0;
}


// HORRIBLE UGLINESS, someone please fix.
// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
inline u16 ToMask(u16 a)
{
	a = a | (a >> 8);
	a = a | (a >> 4);
	a = a | (a >> 2);
	return a | (a >> 1);
}

inline void dsp_increment_addr_reg(int reg)
{
	u16 tmb = ToMask(g_dsp.r[DSP_REG_WR0 + reg]);
	if ((g_dsp.r[reg] & tmb) == tmb)
		g_dsp.r[reg] ^= g_dsp.r[DSP_REG_WR0 + reg];
	else
		g_dsp.r[reg]++;
}

// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
inline void dsp_decrement_addr_reg(int reg)
{
	// This one is easy. Looks like a hw implementation. Increment is worse...
	if ((g_dsp.r[reg] & g_dsp.r[DSP_REG_WR0 + reg]) == 0)
		g_dsp.r[reg] |= g_dsp.r[DSP_REG_WR0 + reg];
	else
		g_dsp.r[reg]--;
}

inline void dsp_increase_addr_reg(int reg, s16 value)
{
	// TODO: DO RIGHT!
	if (value > 0) {
		for (int i = 0; i < value; i++) {
			dsp_increment_addr_reg(reg);
		}
	} else if (value < 0) {
		for (int i = 0; i < (int)(-value); i++) {
			dsp_decrement_addr_reg(reg);
		}
	}
}


// ---------------------------------------------------------------------------------------
// --- reg
// ---------------------------------------------------------------------------------------

inline u16 dsp_op_read_reg(int reg)
{
	switch (reg & 0x1f) {
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		return dsp_reg_load_stack(reg - 0x0c);
	default:
		return g_dsp.r[reg];
	}
}

inline void dsp_op_write_reg(int reg, u16 val)
{
	switch (reg & 0x1f) {
	// 8-bit sign extended registers. Should look at prod.h too...
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		// sign extend from the bottom 8 bits.
		g_dsp.r[reg] = (u16)(s16)(s8)(u8)val;
		break;

	// Stack registers.
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		dsp_reg_store_stack(reg - 0x0c, val);
		break;

	default:
		g_dsp.r[reg] = val;
		break;
	}
}

inline void dsp_conditional_extend_accum(int reg) 
{
	switch (reg) 
	{
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
		{
			// Sign extend into whole accum.
			u16 val = g_dsp.r[reg];
			g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
			g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
		}
	}
}

// ---------------------------------------------------------------------------------------
// --- prod
// ---------------------------------------------------------------------------------------

inline s64 dsp_get_long_prod()
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	s64 val   = (s8)g_dsp.r[0x16];
	val <<= 32;
	s64 low_prod  = g_dsp.r[0x15];
	low_prod += g_dsp.r[0x17];
	low_prod <<= 16;
	low_prod |= g_dsp.r[0x14];
	val += low_prod;
	return val;
}

// For accurate emulation, this is wrong - but the real prod registers behave
// in completely bizarre ways. Probably not meaningful to emulate them accurately.
inline void dsp_set_long_prod(s64 val)
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	g_dsp.r[0x14] = (u16)val;
	val >>= 16;
	g_dsp.r[0x15] = (u16)val;
	val >>= 16;
	g_dsp.r[0x16] = (u16)val;
	g_dsp.r[0x17] = 0;
}

// ---------------------------------------------------------------------------------------
// --- ACC - main accumulators (40-bit)
// ---------------------------------------------------------------------------------------

inline s64 dsp_get_long_acc(int reg)
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	_assert_(reg < 2);
	s64 high = (s64)(s8)g_dsp.r[DSP_REG_ACH0 + reg] << 32;
	u32 mid_low = ((u32)g_dsp.r[DSP_REG_ACM0 + reg] << 16) | g_dsp.r[DSP_REG_ACL0 + reg];
	return high | mid_low;
}

inline void dsp_set_long_acc(int _reg, s64 val)
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	_assert_(_reg < 2);
	g_dsp.r[DSP_REG_ACL0 + _reg] = (u16)val;
	val >>= 16;
	g_dsp.r[DSP_REG_ACM0 + _reg] = (u16)val;
	val >>= 16;
	g_dsp.r[DSP_REG_ACH0 + _reg] = (u16)(s16)(s8)(u8)val;
}


inline s16 dsp_get_acc_l(int _reg)
{
	_assert_(_reg < 2);
	return g_dsp.r[DSP_REG_ACL0 + _reg];
}


inline s16 dsp_get_acc_m(int _reg)
{
	_assert_(_reg < 2);
	return g_dsp.r[DSP_REG_ACM0 + _reg];
}


inline s16 dsp_get_acc_h(int _reg)
{
	_assert_(_reg < 2);
	return g_dsp.r[DSP_REG_ACH0 + _reg];
}


// ---------------------------------------------------------------------------------------
// --- AX - extra accumulators (32-bit)
// ---------------------------------------------------------------------------------------

inline s32 dsp_get_long_acx(int _reg)
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	_assert_(_reg < 2);
	return ((u32)g_dsp.r[DSP_REG_AXH0 + _reg] << 16) | g_dsp.r[DSP_REG_AXL0 + _reg];
}

inline s16 dsp_get_ax_l(int _reg)
{
	_assert_(_reg < 2);
	return (s16)g_dsp.r[DSP_REG_AXL0 + _reg];
}

inline s16 dsp_get_ax_h(int _reg)
{
	_assert_(_reg < 2);
	return (s16)g_dsp.r[DSP_REG_AXH0 + _reg];
}

#endif
