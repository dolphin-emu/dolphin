// Copyright (C) 2003 Dolphin Project.

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

// Additional copyrights go to Duddie and Tratax (c) 2004

#include "../DSPIntCCUtil.h"
#include "../DSPIntUtil.h"
#include "../DSPEmitter.h"
#include "DSPJitUtil.h"
#include "x64Emitter.h"
#include "ABI.h"
using namespace Gen;

// SRS @M, $(0x18+S)
// 0010 1sss mmmm mmmm
// Move value from register $(0x18+D) to data memory pointed by address 
// CR[0-7] | M. That is, the upper 8 bits of the address are the 
// bottom 8 bits from CR, and the lower 8 bits are from the 8-bit immediate.  
// Note: pc+=2 in duddie's doc seems wrong
void DSPEmitter::srs(const UDSPInstruction opc)
{
	u8 reg   = ((opc >> 8) & 0x7) + 0x18;
	u16 *regp = reg_ptr(reg);
	//u16 addr = (g_dsp._r.cr << 8) | (opc & 0xFF);
#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(regp));
	MOVZX(32, 8, EAX, M(&g_dsp._r.cr));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RCX, MDisp(R11,PtrOffset(regp,&g_dsp._r)));
	MOVZX(64, 8, RAX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, cr)));
#endif
	SHL(16, R(EAX), Imm8(8));
	OR(8, R(EAX), Imm8(opc & 0xFF));
	dmem_write();
}

// LRS $(0x18+D), @M
// 0010 0ddd mmmm mmmm
// Move value from data memory pointed by address CR[0-7] | M to register
// $(0x18+D).  That is, the upper 8 bits of the address are the bottom 8 bits
// from CR, and the lower 8 bits are from the 8-bit immediate.
void DSPEmitter::lrs(const UDSPInstruction opc)
{
	u8 reg   = ((opc >> 8) & 0x7) + 0x18;
	u16 *regp = reg_ptr(reg);
	//u16 addr = (g_dsp.r[DSP_REG_CR] << 8) | (opc & 0xFF);
#ifdef _M_IX86 // All32
	MOVZX(32, 8, ECX, M(&g_dsp._r.cr));
	SHL(16, R(ECX), Imm8(8));
	OR(8, R(ECX), Imm8(opc & 0xFF));
	dmem_read();
	MOV(16, M(regp), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 8, RCX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, cr)));
	SHL(16, R(ECX), Imm8(8));
	OR(8, R(ECX), Imm8(opc & 0xFF));
	dmem_read();
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, MDisp(R11,PtrOffset(regp, &g_dsp._r)), R(RAX));
#endif
	dsp_conditional_extend_accum(reg);
}

// LR $D, @M
// 0000 0000 110d dddd
// mmmm mmmm mmmm mmmm
// Move value from data memory pointed by address M to register $D.
// FIXME: Perform additional operation depending on destination register.
void DSPEmitter::lr(const UDSPInstruction opc)
{
	int reg = opc & DSP_REG_MASK;
	u16 address = dsp_imem_read(compilePC + 1);
	dmem_read_imm(address);
	dsp_op_write_reg(reg, EAX);
	dsp_conditional_extend_accum(reg);
}

// SR @M, $S
// 0000 0000 111s ssss
// mmmm mmmm mmmm mmmm
// Store value from register $S to a memory pointed by address M.
// FIXME: Perform additional operation depending on destination register.
void DSPEmitter::sr(const UDSPInstruction opc)
{
	u8 reg   = opc & DSP_REG_MASK;
	u16 address = dsp_imem_read(compilePC + 1);
	dsp_op_read_reg(reg, ECX);
	dmem_write_imm(address);
}

// SI @M, #I
// 0001 0110 mmmm mmmm
// iiii iiii iiii iiii
// Store 16-bit immediate value I to a memory location pointed by address
// M (M is 8-bit value sign extended).
void DSPEmitter::si(const UDSPInstruction opc)
{
	u16 address = (s8)opc;
	u16 imm = dsp_imem_read(compilePC + 1);
	MOV(32, R(ECX), Imm32((u32)imm));
	dmem_write_imm(address);
}

// LRR $D, @$S
// 0001 1000 0ssd dddd
// Move value from data memory pointed by addressing register $S to register $D.
// FIXME: Perform additional operation depending on destination register.
void DSPEmitter::lrr(const UDSPInstruction opc)
{
	u8 sreg = (opc >> 5) & 0x3;
	u8 dreg = opc & 0x1f;

	dsp_op_read_reg(sreg, ECX);
	dmem_read();
	dsp_op_write_reg(dreg, EAX);
	dsp_conditional_extend_accum(dreg);
}

// LRRD $D, @$S
// 0001 1000 1ssd dddd
// Move value from data memory pointed by addressing register $S toregister $D.
// Decrement register $S. 
// FIXME: Perform additional operation depending on destination register.
void DSPEmitter::lrrd(const UDSPInstruction opc)
{
	u8 sreg = (opc >> 5) & 0x3;
	u8 dreg = opc & 0x1f;

	dsp_op_read_reg(sreg, ECX);
	dmem_read();
	dsp_op_write_reg(dreg, EAX);
	dsp_conditional_extend_accum(dreg);
	decrement_addr_reg(sreg);
}

// LRRI $D, @$S
// 0001 1001 0ssd dddd
// Move value from data memory pointed by addressing register $S to register $D.
// Increment register $S. 
// FIXME: Perform additional operation depending on destination register.
void DSPEmitter::lrri(const UDSPInstruction opc)
{
	u8 sreg = (opc >> 5) & 0x3;
	u8 dreg = opc & 0x1f;

	dsp_op_read_reg(sreg, ECX);
	dmem_read();
	dsp_op_write_reg(dreg, EAX);
	dsp_conditional_extend_accum(dreg);
	increment_addr_reg(sreg);
}

// LRRN $D, @$S
// 0001 1001 1ssd dddd
// Move value from data memory pointed by addressing register $S to register $D.
// Add indexing register $(0x4+S) to register $S. 
// FIXME: Perform additional operation depending on destination register.
void DSPEmitter::lrrn(const UDSPInstruction opc)
{
	u8 sreg = (opc >> 5) & 0x3;
	u8 dreg = opc & 0x1f;

	dsp_op_read_reg(sreg, ECX);
	dmem_read();
	dsp_op_write_reg(dreg, EAX);
	dsp_conditional_extend_accum(dreg);
	increase_addr_reg(sreg);
}

// SRR @$D, $S
// 0001 1010 0dds ssss
// Store value from source register $S to a memory location pointed by 
// addressing register $D. 
// FIXME: Perform additional operation depending on source register.
void DSPEmitter::srr(const UDSPInstruction opc)
{
	u8 dreg = (opc >> 5) & 0x3;
	u8 sreg = opc & 0x1f;

	dsp_op_read_reg(sreg, ECX);
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp._r.ar[dreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RAX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ar[dreg])));
#endif
	dmem_write();
}

// SRRD @$D, $S
// 0001 1010 1dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $D. Decrement register $D. 
// FIXME: Perform additional operation depending on source register.
void DSPEmitter::srrd(const UDSPInstruction opc)
{
	u8 dreg = (opc >> 5) & 0x3;
	u8 sreg = opc & 0x1f;

	dsp_op_read_reg(sreg, ECX);
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp._r.ar[dreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RAX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ar[dreg])));
#endif
	dmem_write();
	decrement_addr_reg(dreg);
}

// SRRI @$D, $S
// 0001 1011 0dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $D. Increment register $D. 
// FIXME: Perform additional operation depending on source register.
void DSPEmitter::srri(const UDSPInstruction opc)
{
	u8 dreg = (opc >> 5) & 0x3;
	u8 sreg = opc & 0x1f;

	dsp_op_read_reg(sreg, ECX);
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp._r.ar[dreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RAX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ar[dreg])));
#endif
	dmem_write();
	increment_addr_reg(dreg);
}

// SRRN @$D, $S
// 0001 1011 1dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $D. Add DSP_REG_IX0 register to register $D.
// FIXME: Perform additional operation depending on source register.
void DSPEmitter::srrn(const UDSPInstruction opc)
{
	u8 dreg = (opc >> 5) & 0x3;
	u8 sreg = opc & 0x1f;

	dsp_op_read_reg(sreg, ECX);
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp._r.ar[dreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RAX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ar[dreg])));
#endif
	dmem_write();
	increase_addr_reg(dreg);
}

// ILRR $acD.m, @$arS
// 0000 001d 0001 00ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m.
void DSPEmitter::ilrr(const UDSPInstruction opc)
{
	u16 reg  = opc & 0x3;
	u16 dreg = (opc >> 8) & 1;

#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp._r.ar[reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RCX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ar[reg])));
#endif
	imem_read();
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp._r.ac[dreg].m), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[dreg].m)), R(RAX));
#endif
	dsp_conditional_extend_accum(dreg);
}

// ILRRD $acD.m, @$arS
// 0000 001d 0001 01ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Decrement addressing register $arS.
void DSPEmitter::ilrrd(const UDSPInstruction opc)
{
	u16 reg  = opc & 0x3;
	u16 dreg = (opc >> 8) & 1;

#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp._r.ar[reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RCX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ar[reg])));
#endif
	imem_read();
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp._r.ac[dreg].m), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[dreg].m)), R(RAX));
#endif
	dsp_conditional_extend_accum(dreg);
	decrement_addr_reg(reg);
}

// ILRRI $acD.m, @$S
// 0000 001d 0001 10ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Increment addressing register $arS.
void DSPEmitter::ilrri(const UDSPInstruction opc)
{
	u16 reg  = opc & 0x3;
	u16 dreg = (opc >> 8) & 1;

#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp._r.ar[reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RCX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ar[reg])));
#endif
	imem_read();
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp._r.ac[dreg].m), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[dreg].m)), R(RAX));
#endif
	dsp_conditional_extend_accum(dreg);
	increment_addr_reg(reg);
}

// ILRRN $acD.m, @$arS
// 0000 001d 0001 11ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Add corresponding indexing
// register $ixS to addressing register $arS.
void DSPEmitter::ilrrn(const UDSPInstruction opc)
{
	u16 reg  = opc & 0x3;
	u16 dreg = (opc >> 8) & 1;

#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp._r.ar[reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOVZX(64, 16, RCX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ar[reg])));
#endif
	imem_read();
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp._r.ac[dreg].m), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[dreg].m)), R(RAX));
#endif
	dsp_conditional_extend_accum(dreg);
	increase_addr_reg(reg);
}

