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

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Common.h"
#include "Thunk.h"

#include "../PowerPC.h"
#include "../../Core.h"
#include "../../HW/GPFifo.h"
#include "../../HW/CommandProcessor.h"
#include "../../HW/PixelEngine.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "x64Emitter.h"
#include "ABI.h"

#include "Jit.h"
#include "JitAsm.h"

//#define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

void Jit64::lhax(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	IREmitter::InstLoc val = ibuild.EmitLoad16(addr);
	val = ibuild.EmitSExt16(val);
	ibuild.EmitStoreGReg(val, inst.RD);
}

void Jit64::lXz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)	
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);
	IREmitter::InstLoc val;
	switch (inst.OPCD & ~0x1)
	{
	case 32: val = ibuild.EmitLoad32(addr); break; //lwz	
	case 40: val = ibuild.EmitLoad16(addr); break; //lhz
	case 34: val = ibuild.EmitLoad8(addr);  break; //lbz
	default: PanicAlert("lXz: invalid access size");
	}
	ibuild.EmitStoreGReg(val, inst.RD);
}

void Jit64::lha(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	IREmitter::InstLoc addr =
		ibuild.EmitIntConst((s32)(s16)inst.SIMM_16);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	IREmitter::InstLoc val = ibuild.EmitLoad16(addr);
	val = ibuild.EmitSExt16(val);
	ibuild.EmitStoreGReg(val, inst.RD);
}

void Jit64::lXzx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB);
	if (inst.RA) {
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
		if (inst.SUBOP10 & 32)
			ibuild.EmitStoreGReg(addr, inst.RA);
	}
	IREmitter::InstLoc val;
	switch (inst.SUBOP10 & ~32)
	{
	default: PanicAlert("lXzx: invalid access size");
	case 23:  val = ibuild.EmitLoad32(addr); break; //lwzx
	case 279: val = ibuild.EmitLoad16(addr); break; //lhzx
	case 87:  val = ibuild.EmitLoad8(addr);  break; //lbzx
	}
	ibuild.EmitStoreGReg(val, inst.RD);
}

// Zero cache line.
void Jit64::dcbz(UGeckoInstruction inst)
{
	Default(inst); return;

	// TODO!
#if 0
	if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreOff)
		{Default(inst); return;} // turn off from debugger	
	INSTRUCTION_START;
		MOV(32, R(EAX), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(EAX), gpr.R(inst.RA));
	AND(32, R(EAX), Imm32(~31));
	XORPD(XMM0, R(XMM0));
#ifdef _M_X64
	MOVAPS(MComplex(EBX, EAX, SCALE_1, 0), XMM0);
	MOVAPS(MComplex(EBX, EAX, SCALE_1, 16), XMM0);
#else
	AND(32, R(EAX), Imm32(Memory::MEMVIEW32_MASK));
	MOVAPS(MDisp(EAX, (u32)Memory::base), XMM0);
	MOVAPS(MDisp(EAX, (u32)Memory::base + 16), XMM0);
#endif
#endif
}

void Jit64::stX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16),
			   value = ibuild.EmitLoadGReg(inst.RS);
	if (inst.RA)
		addr = ibuild.EmitAdd(ibuild.EmitLoadGReg(inst.RA), addr);
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);
	switch (inst.OPCD & ~1)
	{
	case 36: ibuild.EmitStore32(value, addr); break; //stw
	case 44: ibuild.EmitStore16(value, addr); break; //sth
	case 38: ibuild.EmitStore8(value, addr); break;  //stb
	default: _assert_msg_(DYNA_REC, 0, "AWETKLJASDLKF"); return;
	}
}

void Jit64::stXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB),
			   value = ibuild.EmitLoadGReg(inst.RS);
	addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.SUBOP10 & 32)
		ibuild.EmitStoreGReg(addr, inst.RA);
	switch (inst.SUBOP10 & ~32)
	{
	case 151: ibuild.EmitStore32(value, addr); break; //stw
	case 407: ibuild.EmitStore16(value, addr); break; //sth
	case 215: ibuild.EmitStore8(value, addr); break;  //stb
	default: _assert_msg_(DYNA_REC, 0, "AWETKLJASDLKF"); return;
	}
}

// A few games use these heavily in video codecs.
void Jit64::lmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	for (int i = inst.RD; i < 32; i++)
	{
		IREmitter::InstLoc val = ibuild.EmitLoad32(addr);
		ibuild.EmitStoreGReg(val, i);
		addr = ibuild.EmitAdd(addr, ibuild.EmitIntConst(4));
	}
}

void Jit64::stmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	for (int i = inst.RD; i < 32; i++)
	{
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(i);
		ibuild.EmitStore32(val, addr);
		addr = ibuild.EmitAdd(addr, ibuild.EmitIntConst(4));
	}
}
