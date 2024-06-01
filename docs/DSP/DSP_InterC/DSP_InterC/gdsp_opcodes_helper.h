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

#include "Globals.h"

#include "gdsp_opcodes.h"
#include "gdsp_memory.h"
#include "gdsp_interpreter.h"
#include "gdsp_registers.h"
#include "gdsp_ext_op.h"

// ---------------------------------------------------------------------------------------
//
// --- SR
//
// ---------------------------------------------------------------------------------------

inline void dsp_SR_set_flag(uint8 flag)
{
	g_dsp.r[R_SR] |= (1 << flag);
}


inline bool dsp_SR_is_flag_set(uint8 flag)
{
	return((g_dsp.r[R_SR] & (1 << flag)) > 0);
}


// ---------------------------------------------------------------------------------------
//
// --- reg
//
// ---------------------------------------------------------------------------------------

inline uint16 dsp_op_read_reg(uint8 reg)
{
	uint16 val;

	switch (reg & 0x1f)
	{
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

	return(val);
}


inline void dsp_op_write_reg(uint8 reg, uint16 val)
{
	switch (reg & 0x1f)
	{
	    case 0x0c:
	    case 0x0d:
	    case 0x0e:
	    case 0x0f:
		    dsp_reg_store_stack(reg - 0x0c, val);
		    break;

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


inline sint64 dsp_get_long_prod()
{
	sint64 val;
	sint64 low_prod;
	val   = (sint8)g_dsp.r[0x16];
	val <<= 32;
	low_prod  = g_dsp.r[0x15];
	low_prod += g_dsp.r[0x17];
	low_prod <<= 16;
	low_prod |= g_dsp.r[0x14];
	val += low_prod;
	return(val);
}


inline void dsp_set_long_prod(sint64 val)
{
	g_dsp.r[0x14] = (uint16)val;
	val >>= 16;
	g_dsp.r[0x15] = (uint16)val;
	val >>= 16;
	g_dsp.r[0x16] = (uint16)val;
	g_dsp.r[0x17] = 0;
}


// ---------------------------------------------------------------------------------------
//
// --- acc
//
// ---------------------------------------------------------------------------------------

inline sint64 dsp_get_long_acc(uint8 reg)
{
	_dbg_assert_(reg < 2);
	sint64 val;
	sint64 low_acc;
	val       = (sint8)g_dsp.r[0x10 + reg];
	val     <<= 32;
	low_acc   = g_dsp.r[0x1e + reg];
	low_acc <<= 16;
	low_acc  |= g_dsp.r[0x1c + reg];
	val |= low_acc;
	return(val);
}


inline uint64 dsp_get_ulong_acc(uint8 reg)
{
	_dbg_assert_(reg < 2);
	uint64 val;
	uint64 low_acc;
	val       = (uint8)g_dsp.r[0x10 + reg];
	val     <<= 32;
	low_acc   = g_dsp.r[0x1e + reg];
	low_acc <<= 16;
	low_acc  |= g_dsp.r[0x1c + reg];
	val |= low_acc;
	return(val);
}


inline void dsp_set_long_acc(uint8 _reg, sint64 val)
{
	_dbg_assert_(_reg < 2);
	g_dsp.r[0x1c + _reg] = (uint16)val;
	val >>= 16;
	g_dsp.r[0x1e + _reg] = (uint16)val;
	val >>= 16;
	g_dsp.r[0x10 + _reg] = (uint16)val;
}


inline sint16 dsp_get_acc_l(uint8 _reg)
{
	_dbg_assert_(_reg < 2);
	return(g_dsp.r[0x1c + _reg]);
}


inline sint16 dsp_get_acc_m(uint8 _reg)
{
	_dbg_assert_(_reg < 2);
	return(g_dsp.r[0x1e + _reg]);
}


inline sint16 dsp_get_acc_h(uint8 _reg)
{
	_dbg_assert_(_reg < 2);
	return(g_dsp.r[0x10 + _reg]);
}


// ---------------------------------------------------------------------------------------
//
// --- acx
//
// ---------------------------------------------------------------------------------------


inline sint64 dsp_get_long_acx(uint8 _reg)
{
	_dbg_assert_(_reg < 2);
	sint64 val = (sint16)g_dsp.r[0x1a + _reg];
	val <<= 16;
	sint64 low_acc = g_dsp.r[0x18 + _reg];
	val |= low_acc;
	return(val);
}


inline sint16 dsp_get_ax_l(uint8 _reg)
{
	_dbg_assert_(_reg < 2);
	return(g_dsp.r[0x18 + _reg]);
}


inline sint16 dsp_get_ax_h(uint8 _reg)
{
	_dbg_assert_(_reg < 2);
	return(g_dsp.r[0x1a + _reg]);
}


#endif
