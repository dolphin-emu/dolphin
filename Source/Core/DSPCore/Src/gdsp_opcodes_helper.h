/*====================================================================

   filename:     opcodes.h
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

#ifndef _GDSP_OPCODES_HELPER_H
#define _GDSP_OPCODES_HELPER_H

#include "Common.h"

#include "DSPInterpreter.h"

#include "gdsp_memory.h"
#include "gdsp_interpreter.h"
#include "gdsp_registers.h"
#include "gdsp_ext_op.h"

// ---------------------------------------------------------------------------------------
//
// --- SR
//
// ---------------------------------------------------------------------------------------

inline void dsp_SR_set_flag(int flag)
{
	g_dsp.r[DSP_REG_SR] |= (1 << flag);
}

inline bool dsp_SR_is_flag_set(int flag)
{
	return (g_dsp.r[DSP_REG_SR] & (1 << flag)) != 0;
}


// ---------------------------------------------------------------------------------------
//
// --- reg
//
// ---------------------------------------------------------------------------------------

inline u16 dsp_op_read_reg(u8 reg)
{
	u16 val;
	
	switch (reg & 0x1f) {
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:

		val = dsp_reg_load_stack(reg - 0x0c);
		break;

	default:
		val = g_dsp.r[reg];
		break;
	}

	return val;
}


inline void dsp_op_write_reg(u8 reg, u16 val)
{
	switch (reg & 0x1f) {


	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		dsp_reg_store_stack(reg - 0x0c, val);
		break;
#if 0 // FIXME
	case 0x1e: // AC0.M
	case 0x1f: // AC1.M
		// in "s16 mode", LRI $AC0.M, xxx  will set AC0.L and AC0.H to 0, 
		// while it won't in "s40 mode".
		if (g_dsp.r[DSP_REG_SR] & SR_16_BIT) {
				g_dsp.r[reg - 0x2] = 0; // L
				g_dsp.r[reg - 0xe] = 0; // H
		}
		
		g_dsp.r[reg] = val;
		break;


	case 0x1c: // AC0.L
	case 0x1d: // AC1.L
		
		if (g_dsp.r[DSP_REG_SR] & SR_16_BIT) {
				g_dsp.r[reg + 0x2] = 0; // M
				g_dsp.r[reg - 0xc] = 0; // H
		}
		
		g_dsp.r[reg] = val;
		break;

	case 0x10: // AC0.H
	case 0x11: // AC1.H
		if (g_dsp.r[DSP_REG_SR] & SR_16_BIT) {
				g_dsp.r[reg + 0xc] = 0; // L
				g_dsp.r[reg + 0xe] = 0; // M
		}
		
		g_dsp.r[reg] = val;
		break;
#endif


	default:
		g_dsp.r[reg] = val;
		break;
	}
}


// ---------------------------------------------------------------------------------------
//
// --- prod
//
// ---------------------------------------------------------------------------------------


inline s64 dsp_get_long_prod()
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	s64 val;
	s64 low_prod;
	val   = (s8)g_dsp.r[0x16];
	val <<= 32;
	low_prod  = g_dsp.r[0x15];
	low_prod += g_dsp.r[0x17];
	low_prod <<= 16;
	low_prod |= g_dsp.r[0x14];
	val += low_prod;
	return val;
}

// For accurate emulation, this is wrong - it should take the two multiplicands
// as input and set the two mid stages accordingly. most likely it's doing something
// pretty simple.
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
// --- acc
// ---------------------------------------------------------------------------------------

inline s64 dsp_get_long_acc(int reg)
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	_assert_(reg < 2);
	s64 high = (s64)(s8)g_dsp.r[0x10 + reg] << 32;
	u32 mid_low = ((u32)g_dsp.r[0x1e + reg] << 16) | g_dsp.r[0x1c + reg];
	return high | mid_low;
}

inline void dsp_set_long_acc(int _reg, s64 val)
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	_assert_(_reg < 2);
	g_dsp.r[0x1c + _reg] = (u16)val;
	val >>= 16;
	g_dsp.r[0x1e + _reg] = (u16)val;
	val >>= 16;
	g_dsp.r[0x10 + _reg] = (u16)(s16)(s8)(u8)val;
}


inline s16 dsp_get_acc_l(int _reg)
{
	_assert_(_reg < 2);
	return g_dsp.r[0x1c + _reg];
}


inline s16 dsp_get_acc_m(int _reg)
{
	_assert_(_reg < 2);
	return g_dsp.r[0x1e + _reg];
}


inline s16 dsp_get_acc_h(int _reg)
{
	_assert_(_reg < 2);
	return g_dsp.r[0x10 + _reg];
}


// ---------------------------------------------------------------------------------------
//
// --- acx
//
// ---------------------------------------------------------------------------------------


inline s64 dsp_get_long_acx(int _reg)
{
#if PROFILE
	ProfilerAddDelta(g_dsp.err_pc, 1);
#endif

	_assert_(_reg < 2);
	s64 val = (s16)g_dsp.r[0x1a + _reg];
	val <<= 16;
	s64 low_acc = g_dsp.r[0x18 + _reg];
	val |= low_acc;
	return val;
}

inline s16 dsp_get_ax_l(int _reg)
{
	_assert_(_reg < 2);
	return g_dsp.r[0x18 + _reg];
}

inline s16 dsp_get_ax_h(int _reg)
{
	_assert_(_reg < 2);
	return g_dsp.r[0x1a + _reg];
}

#endif
