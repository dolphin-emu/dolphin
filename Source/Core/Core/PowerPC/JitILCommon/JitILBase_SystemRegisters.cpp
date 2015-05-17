// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/JitILCommon/JitILBase.h"

void JitILBase::mtspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);
	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

	switch (iIndex)
	{
		case SPR_TL:
		case SPR_TU:
			FALLBACK_IF(true);
		case SPR_LR:
			ibuild.EmitStoreLink(ibuild.EmitLoadGReg(inst.RD));
			return;
		case SPR_CTR:
			ibuild.EmitStoreCTR(ibuild.EmitLoadGReg(inst.RD));
			return;
		case SPR_GQR0:
		case SPR_GQR0 + 1:
		case SPR_GQR0 + 2:
		case SPR_GQR0 + 3:
		case SPR_GQR0 + 4:
		case SPR_GQR0 + 5:
		case SPR_GQR0 + 6:
		case SPR_GQR0 + 7:
			ibuild.EmitStoreGQR(ibuild.EmitLoadGReg(inst.RD), iIndex - SPR_GQR0);
			return;
		case SPR_SRR0:
		case SPR_SRR1:
			ibuild.EmitStoreSRR(ibuild.EmitLoadGReg(inst.RD), iIndex - SPR_SRR0);
			return;
		default:
			FALLBACK_IF(true);
	}
}

void JitILBase::mfspr(UGeckoInstruction inst)
{
	INSTRUCTION_START
		JITDISABLE(bJITSystemRegistersOff);
		u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
	switch (iIndex)
	{
	case SPR_TL:
	case SPR_TU:
		FALLBACK_IF(true);
	case SPR_LR:
		ibuild.EmitStoreGReg(ibuild.EmitLoadLink(), inst.RD);
		return;
	case SPR_CTR:
		ibuild.EmitStoreGReg(ibuild.EmitLoadCTR(), inst.RD);
		return;
	case SPR_GQR0:
	case SPR_GQR0 + 1:
	case SPR_GQR0 + 2:
	case SPR_GQR0 + 3:
	case SPR_GQR0 + 4:
	case SPR_GQR0 + 5:
	case SPR_GQR0 + 6:
	case SPR_GQR0 + 7:
		ibuild.EmitStoreGReg(ibuild.EmitLoadGQR(iIndex - SPR_GQR0), inst.RD);
		return;
	default:
		FALLBACK_IF(true);
	}
}


// =======================================================================================
// Don't interpret this, if we do we get thrown out
// --------------
void JitILBase::mtmsr(UGeckoInstruction inst)
{
	ibuild.EmitStoreMSR(ibuild.EmitLoadGReg(inst.RS), ibuild.EmitIntConst(js.compilerPC));
	ibuild.EmitBranchUncond(ibuild.EmitIntConst(js.compilerPC + 4));
}
// ==============


void JitILBase::mfmsr(UGeckoInstruction inst)
{
	INSTRUCTION_START
		JITDISABLE(bJITSystemRegistersOff);
		ibuild.EmitStoreGReg(ibuild.EmitLoadMSR(), inst.RD);
}

void JitILBase::mftb(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	JITDISABLE(bJITSystemRegistersOff);
		mfspr(inst);
}

void JitILBase::mfcr(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	JITDISABLE(bJITSystemRegistersOff);

	IREmitter::InstLoc d = ibuild.EmitIntConst(0);
	for (int i = 0; i < 8; ++i)
	{
		IREmitter::InstLoc cr = ibuild.EmitLoadCR(i);
		cr = ibuild.EmitConvertFromFastCR(cr);
		cr = ibuild.EmitShl(cr, ibuild.EmitIntConst(28 - 4 * i));
		d = ibuild.EmitOr(d, cr);
	}
	ibuild.EmitStoreGReg(d, inst.RD);
}

void JitILBase::mtcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	JITDISABLE(bJITSystemRegistersOff);

	IREmitter::InstLoc s = ibuild.EmitLoadGReg(inst.RS);
	for (int i = 0; i < 8; ++i)
	{
		if (inst.CRM & (0x80 >> i))
		{
			IREmitter::InstLoc value;
			value = ibuild.EmitShrl(s, ibuild.EmitIntConst(28 - i * 4));
			value = ibuild.EmitAnd(value, ibuild.EmitIntConst(0xF));
			value = ibuild.EmitConvertToFastCR(value);
			ibuild.EmitStoreCR(value, i);
		}
	}
}

void JitILBase::mcrf(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	if (inst.CRFS != inst.CRFD)
	{
		ibuild.EmitStoreCR(ibuild.EmitLoadCR(inst.CRFS), inst.CRFD);
	}
}

void JitILBase::crXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITSystemRegistersOff);

	// Get bit CRBA in EAX aligned with bit CRBD
	const int shiftA = (inst.CRBD & 3) - (inst.CRBA & 3);
	IREmitter::InstLoc eax = ibuild.EmitLoadCR(inst.CRBA >> 2);
	eax = ibuild.EmitConvertFromFastCR(eax);
	if (shiftA < 0)
		eax = ibuild.EmitShl(eax, ibuild.EmitIntConst(-shiftA));
	else if (shiftA > 0)
		eax = ibuild.EmitShrl(eax, ibuild.EmitIntConst(shiftA));

	// Get bit CRBB in ECX aligned with bit CRBD
	const int shiftB = (inst.CRBD & 3) - (inst.CRBB & 3);
	IREmitter::InstLoc ecx = ibuild.EmitLoadCR(inst.CRBB >> 2);
	ecx = ibuild.EmitConvertFromFastCR(ecx);
	if (shiftB < 0)
		ecx = ibuild.EmitShl(ecx, ibuild.EmitIntConst(-shiftB));
	else if (shiftB > 0)
		ecx = ibuild.EmitShrl(ecx, ibuild.EmitIntConst(shiftB));

	// Compute combined bit
	const unsigned subop = inst.SUBOP10;
	switch (subop)
	{
		case 257: // crand
			eax = ibuild.EmitAnd(eax, ecx);
			break;
		case 129: // crandc
			ecx = ibuild.EmitNot(ecx);
			eax = ibuild.EmitAnd(eax, ecx);
			break;
		case 289: // creqv
			eax = ibuild.EmitXor(eax, ecx);
			eax = ibuild.EmitNot(eax);
			break;
		case 225: // crnand
			eax = ibuild.EmitAnd(eax, ecx);
			eax = ibuild.EmitNot(eax);
			break;
		case 33:  // crnor
			eax = ibuild.EmitOr(eax, ecx);
			eax = ibuild.EmitNot(eax);
			break;
		case 449: // cror
			eax = ibuild.EmitOr(eax, ecx);
			break;
		case 417: // crorc
			ecx = ibuild.EmitNot(ecx);
			eax = ibuild.EmitOr(eax, ecx);
			break;
		case 193: // crxor
			eax = ibuild.EmitXor(eax, ecx);
			break;
		default:
			PanicAlert("crXX: invalid instruction");
			break;
	}

	// Store result bit in CRBD
	eax = ibuild.EmitAnd(eax, ibuild.EmitIntConst(0x8 >> (inst.CRBD & 3)));
	IREmitter::InstLoc bd = ibuild.EmitLoadCR(inst.CRBD >> 2);
	bd = ibuild.EmitConvertFromFastCR(bd);
	bd = ibuild.EmitAnd(bd, ibuild.EmitIntConst(~(0x8 >> (inst.CRBD & 3))));
	bd = ibuild.EmitOr(bd, eax);
	bd = ibuild.EmitConvertToFastCR(bd);
	ibuild.EmitStoreCR(bd, inst.CRBD >> 2);
}
