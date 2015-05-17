/*====================================================================

   filename:     opcodes.h
   project:      GameCube DSP Tool (gcdsp)
   created:      2005.03.04
   mail:         duddie@walla.com

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   ====================================================================*/

#pragma once

#include "Core/DSP/DSPCommon.h"

// Extended opcode support.
// Many opcode have the lower 0xFF (some only 0x7f) free - there, an opcode extension
// can be stored.

namespace DSPInterpreter
{
namespace Ext
{
void l(const UDSPInstruction opc);
void ln(const UDSPInstruction opc);
void ls(const UDSPInstruction opc);
void lsn(const UDSPInstruction opc);
void lsm(const UDSPInstruction opc);
void lsnm(const UDSPInstruction opc);
void sl(const UDSPInstruction opc);
void sln(const UDSPInstruction opc);
void slm(const UDSPInstruction opc);
void slnm(const UDSPInstruction opc);
void s(const UDSPInstruction opc);
void sn(const UDSPInstruction opc);
void ld(const UDSPInstruction opc);
void ldax(const UDSPInstruction opc);
void ldn(const UDSPInstruction opc);
void ldaxn(const UDSPInstruction opc);
void ldm(const UDSPInstruction opc);
void ldaxm(const UDSPInstruction opc);
void ldnm(const UDSPInstruction opc);
void ldaxnm(const UDSPInstruction opc);
void mv(const UDSPInstruction opc);
void dr(const UDSPInstruction opc);
void ir(const UDSPInstruction opc);
void nr(const UDSPInstruction opc);
void nop(const UDSPInstruction opc);

} // end namespace Ext
} // end namespace DSPinterpeter
