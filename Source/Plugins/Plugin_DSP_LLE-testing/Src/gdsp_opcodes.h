// Copyright (C) 2003-2009 Dolphin Project.

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

/*====================================================================

   filename:     gdsp_opcodes.h
   project:      GCemu
   created:      2004-6-18
   mail:		  duddie@walla.com

   Copyright (c) 2005 Duddie & Tratax

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
#ifndef _GDSP_OPCODES_H
#define _GDSP_OPCODES_H

#include "Globals.h"

void dsp_op0(u16 opc);
void dsp_op1(u16 opc);
void dsp_op2(u16 opc);
void dsp_op3(u16 opc);
void dsp_op4(u16 opc);
void dsp_op5(u16 opc);
void dsp_op6(u16 opc);
void dsp_op7(u16 opc);
void dsp_op8(u16 opc);
void dsp_op9(u16 opc);
void dsp_opab(u16 opc);
void dsp_opcd(u16 opc);
void dsp_ope(u16 opc);
void dsp_opf(u16 opc);


#define R_SR            0x13

#define FLAG_ENABLE_INTERUPT    11

#endif
