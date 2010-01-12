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

namespace DSPInterpreter
{
namespace Ext
{ 
void l(const UDSPInstruction& opc);
void ln(const UDSPInstruction& opc);
void ls(const UDSPInstruction& opc);
void lsn(const UDSPInstruction& opc);
void lsm(const UDSPInstruction& opc);
void lsnm(const UDSPInstruction& opc);
void sl(const UDSPInstruction& opc);
void sln(const UDSPInstruction& opc);
void slm(const UDSPInstruction& opc);
void slnm(const UDSPInstruction& opc);
void s(const UDSPInstruction& opc);
void sn(const UDSPInstruction& opc);
void ld(const UDSPInstruction& opc);
void ldn(const UDSPInstruction& opc);
void ldm(const UDSPInstruction& opc);
void ldnm(const UDSPInstruction& opc);
void mv(const UDSPInstruction& opc);
void dr(const UDSPInstruction& opc);
void ir(const UDSPInstruction& opc);
void nr(const UDSPInstruction& opc);
void nop(const UDSPInstruction& opc);
 
} // end namespace Ext
} // end namespace DSPinterpeter

// Needs comments.
inline void writeToBackLog(int i, int idx, u16 value)
{
	writeBackLog[i] = value;
	writeBackLogIdx[i] = idx;
}

#endif
