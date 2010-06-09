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

void dsp_op0(uint16 opc);
void dsp_op1(uint16 opc);
void dsp_op2(uint16 opc);
void dsp_op3(uint16 opc);
void dsp_op4(uint16 opc);
void dsp_op5(uint16 opc);
void dsp_op6(uint16 opc);
void dsp_op7(uint16 opc);
void dsp_op8(uint16 opc);
void dsp_op9(uint16 opc);
void dsp_opab(uint16 opc);
void dsp_opcd(uint16 opc);
void dsp_ope(uint16 opc);
void dsp_opf(uint16 opc);


#define R_SR            0x13

#define FLAG_ENABLE_INTERUPT    11

#endif
