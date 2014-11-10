// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/PowerPC/JitILCommon/JitILBase.h"

void JitILBase::lhax(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(js.memcheck);

	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	IREmitter::InstLoc val = ibuild.EmitLoad16(addr);
	val = ibuild.EmitSExt16(val);
	ibuild.EmitStoreGReg(val, inst.RD);
}

void JitILBase::lXz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(js.memcheck);

	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);

	IREmitter::InstLoc val;

	// Idle Skipping. This really should be done somewhere else.
	// Either lower in the IR or higher in PPCAnalyist
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle &&
		PowerPC::GetState() != PowerPC::CPU_STEPPING &&
		inst.OPCD == 32 && // Lwx
		(inst.hex & 0xFFFF0000) == 0x800D0000 &&
		(Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x28000000 ||
		(SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x2C000000)) &&
		Memory::ReadUnchecked_U32(js.compilerPC + 8) == 0x4182fff8)
	{
		val = ibuild.EmitLoad32(addr);
		ibuild.EmitIdleBranch(val, ibuild.EmitIntConst(js.compilerPC));
		ibuild.EmitStoreGReg(val, inst.RD);
		return;
	}

	switch (inst.OPCD & ~0x1)
	{
	case 32: // lwz
		val = ibuild.EmitLoad32(addr);
		break;
	case 40: // lhz
		val = ibuild.EmitLoad16(addr);
		break;
	case 34: // lbz
		val = ibuild.EmitLoad8(addr);
		break;
	default:
		PanicAlert("lXz: invalid access size");
		val = nullptr;
		break;
	}

	ibuild.EmitStoreGReg(val, inst.RD);
}

void JitILBase::lbzu(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	const IREmitter::InstLoc uAddress = ibuild.EmitAdd(ibuild.EmitLoadGReg(inst.RA), ibuild.EmitIntConst((int)inst.SIMM_16));
	const IREmitter::InstLoc temp = ibuild.EmitLoad8(uAddress);
	ibuild.EmitStoreGReg(temp, inst.RD);
	ibuild.EmitStoreGReg(uAddress, inst.RA);
}

void JitILBase::lha(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(js.memcheck);

	IREmitter::InstLoc addr = ibuild.EmitIntConst((s32)(s16)inst.SIMM_16);

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	IREmitter::InstLoc val = ibuild.EmitLoad16(addr);
	val = ibuild.EmitSExt16(val);
	ibuild.EmitStoreGReg(val, inst.RD);
}

void JitILBase::lXzx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(js.memcheck);

	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB);

	if (inst.RA)
	{
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
		if (inst.SUBOP10 & 32)
			ibuild.EmitStoreGReg(addr, inst.RA);
	}

	IREmitter::InstLoc val;
	switch (inst.SUBOP10 & ~32)
	{
	default:
		PanicAlert("lXzx: invalid access size");
	case 23:  // lwzx
		val = ibuild.EmitLoad32(addr);
		break;
	case 279: // lhzx
		val = ibuild.EmitLoad16(addr);
		break;
	case 87:  // lbzx
		val = ibuild.EmitLoad8(addr);
		break;
	}
	ibuild.EmitStoreGReg(val, inst.RD);
}

void JitILBase::dcbst(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	// If the dcbst instruction is preceded by dcbt, it is flushing a prefetched
	// memory location.  Do not invalidate the JIT cache in this case as the memory
	// will be the same.
	// dcbt = 0x7c00022c
	FALLBACK_IF((Memory::ReadUnchecked_U32(js.compilerPC - 4) & 0x7c00022c) != 0x7c00022c);
}

// Zero cache line.
void JitILBase::dcbz(UGeckoInstruction inst)
{
	FALLBACK_IF(true);

	// TODO!
#if 0
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITOff || SConfig::GetInstance().m_LocalCoreStartupParameter.bJITLoadStoreOff)
	{
		Default(inst);
		return;
	}
	INSTRUCTION_START;
		MOV(32, R(RSCRATCH), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(RSCRATCH), gpr.R(inst.RA));
	AND(32, R(RSCRATCH), Imm32(~31));
	PXOR(XMM0, R(XMM0));
	MOVAPS(MComplex(RMEM, RSCRATCH, SCALE_1, 0), XMM0);
	MOVAPS(MComplex(RMEM, RSCRATCH, SCALE_1, 16), XMM0);
#endif
}

void JitILBase::stX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(js.memcheck);

	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16);
	IREmitter::InstLoc value = ibuild.EmitLoadGReg(inst.RS);

	if (inst.RA)
		addr = ibuild.EmitAdd(ibuild.EmitLoadGReg(inst.RA), addr);
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);

	switch (inst.OPCD & ~1)
	{
	case 36: // stw
		ibuild.EmitStore32(value, addr);
		break;
	case 44: // sth
		ibuild.EmitStore16(value, addr);
		break;
	case 38: // stb
		ibuild.EmitStore8(value, addr);
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "stX: Invalid access size.");
		return;
	}
}

void JitILBase::stXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(js.memcheck);

	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB);
	IREmitter::InstLoc value = ibuild.EmitLoadGReg(inst.RS);

	addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	if (inst.SUBOP10 & 32)
		ibuild.EmitStoreGReg(addr, inst.RA);

	switch (inst.SUBOP10 & ~32)
	{
	case 151: // stw
		ibuild.EmitStore32(value, addr);
		break;
	case 407: // sth
		ibuild.EmitStore16(value, addr);
		break;
	case 215: // stb
		ibuild.EmitStore8(value, addr);
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "stXx: Invalid store size.");
		return;
	}
}

// A few games use these heavily in video codecs. (GFZP01 @ 0x80020E18)
void JitILBase::lmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(js.memcheck);

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

void JitILBase::stmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	FALLBACK_IF(js.memcheck);

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

void JitILBase::icbi(UGeckoInstruction inst)
{
	FallBackToInterpreter(inst);
	ibuild.EmitBranchUncond(ibuild.EmitIntConst(js.compilerPC + 4));
}
