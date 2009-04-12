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

#ifndef _GDSP_EXT_OP_H
#define _GDSP_EXT_OP_H

#include "DSPTables.h"

// Extended opcode support.
// Many opcode have the lower 0xFF free - there, an opcode extension
// can be stored. The ones that must be executed before the operation
// is handled as a prologue, the ones that must be executed afterwards
// is handled as an epilogue.

void dsp_op_ext_ops_pro(const UDSPInstruction& opc);  // run any prologs
void dsp_op_ext_ops_epi(const UDSPInstruction& opc);  // run any epilogs


#endif
