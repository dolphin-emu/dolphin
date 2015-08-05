// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitLLVM/Jit.h"

using namespace llvm;

Value* JitLLVM::GetCRBit(LLVMFunction* func, int field, int bit, bool negate)
{
	IRBuilder<>* builder = func->GetBuilder();

	Value* cr = func->LoadCR(field);
	Value* res;
	Value* tmp;
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		tmp = builder->CreateAnd(cr, builder->getInt64(1ULL << 61));
		if (negate)
			res = builder->CreateICmpEQ(tmp, builder->getInt64(0));
		else
			res = builder->CreateICmpNE(tmp, builder->getInt64(0));
		break;

	case CR_EQ_BIT:  // check bits 31-0 == 0
		tmp = builder->CreateAnd(cr, builder->getInt64((1ULL << 31) - 1));
		if (negate)
			res = builder->CreateICmpNE(tmp, builder->getInt64(0));
		else
			res = builder->CreateICmpEQ(tmp, builder->getInt64(0));
		break;

	case CR_GT_BIT:  // check val > 0
		if (negate)
			res = builder->CreateICmpSLE(cr, builder->getInt64(0));
		else
			res = builder->CreateICmpSGT(cr, builder->getInt64(0));
		break;

	case CR_LT_BIT:  // check bit 62 set
		tmp = builder->CreateAnd(cr, builder->getInt64(1ULL << 62));
		if (negate)
			res = builder->CreateICmpEQ(tmp, builder->getInt64(0));
		else
			res = builder->CreateICmpNE(tmp, builder->getInt64(0));
		break;

	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
		res = UndefValue::get(builder->getInt64Ty());
	}

	return res;
}

void JitLLVM::SetCRBit(LLVMFunction* func, int field, int bit, Value* val)
{
	IRBuilder<>* builder = func->GetBuilder();

	Value* cr = func->LoadCR(field);
	Value* res;
	Value* tmp;
	switch (bit)
	{
	case CR_SO_BIT:
		res = builder->CreateAnd(cr, builder->getInt64(~(1ULL << 61)));
		tmp = builder->CreateOr(cr, builder->getInt64(1ULL << 61));
		res = builder->CreateSelect(val, tmp, res);
		break;

	case CR_EQ_BIT:
		res = builder->CreateLShr(cr, builder->getInt64(32));
		res = builder->CreateShl(res, builder->getInt64(32));

		tmp = builder->CreateOr(res, builder->getInt64(1));
		res = builder->CreateSelect(val, tmp, res);
		break;

	case CR_GT_BIT:
		res = builder->CreateAnd(cr, builder->getInt64(~(1ULL << 63)));
		tmp = builder->CreateOr(cr, builder->getInt64(1ULL << 63));
		res = builder->CreateSelect(val, tmp, res);
		break;

	case CR_LT_BIT:
		res = builder->CreateAnd(cr, builder->getInt64(~(1ULL << 62)));
		tmp = builder->CreateOr(cr, builder->getInt64(1ULL << 62));
		res = builder->CreateSelect(val, tmp, res);
		break;
	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
		res = UndefValue::get(builder->getInt64Ty());
	}
	func->StoreCR(res, field);
}

void JitLLVM::SetCRBit(LLVMFunction* func, int field, int bit)
{
	IRBuilder<>* builder = func->GetBuilder();

	Value* cr = func->LoadCR(field);
	Value* res;
	switch (bit)
	{
	case CR_SO_BIT:
		res = builder->CreateOr(cr, builder->getInt64(1ULL << 61));
		break;

	case CR_EQ_BIT:
		res = builder->CreateLShr(cr, builder->getInt64(32));
		res = builder->CreateShl(res, builder->getInt64(32));
		break;

	case CR_GT_BIT:
		res = builder->CreateAnd(cr, builder->getInt64(~(1ULL << 63)));
		break;

	case CR_LT_BIT:
		res = builder->CreateOr(cr, builder->getInt64(1ULL << 62));
		break;
	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
		res = UndefValue::get(builder->getInt64Ty());
	}
	func->StoreCR(res, field);
}

void JitLLVM::ClearCRBit(LLVMFunction* func, int field, int bit)
{
	IRBuilder<>* builder = func->GetBuilder();

	Value* cr = func->LoadCR(field);
	Value* res;
	switch (bit)
	{
	case CR_SO_BIT:
		res = builder->CreateAnd(cr, builder->getInt64(~(1ULL << 61)));
		break;

	case CR_EQ_BIT:
		res = builder->CreateOr(cr, builder->getInt64(1));
		break;

	case CR_GT_BIT:
		res = builder->CreateOr(cr, builder->getInt64(1ULL << 63));
		break;

	case CR_LT_BIT:
		res = builder->CreateAnd(cr, builder->getInt64(~(1ULL << 62)));
		break;
	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
		res = UndefValue::get(builder->getInt64Ty());
	}
	func->StoreCR(res, field);
	// We don't need to set bit 32; the cases where that's needed only come up when setting bits, not clearing.
}

void JitLLVM::mfspr(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);

	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

	switch (iIndex)
	{
	case SPR_XER:
		// XXX
	case SPR_WPAR:
	case SPR_DEC:
		LLVM_FALLBACK_IF(true);
	case SPR_TL:
	case SPR_TU:
		// XXX:
	default:
		Value* spr_val = func->LoadSPR(iIndex * 4);
		func->StoreGPR(spr_val, inst.RD);
		break;
	}
}

void JitLLVM::mftb(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);
	mfspr(func, inst);
}

void JitLLVM::mtspr(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);

	u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

	switch (iIndex)
	{
	case SPR_DMAU:

	case SPR_SPRG0:
	case SPR_SPRG1:
	case SPR_SPRG2:
	case SPR_SPRG3:

	case SPR_SRR0:
	case SPR_SRR1:
		// These are safe to do the easy way, see the bottom of this function.
	break;

	case SPR_LR:
	case SPR_CTR:
	case SPR_GQR0:
	case SPR_GQR0 + 1:
	case SPR_GQR0 + 2:
	case SPR_GQR0 + 3:
	case SPR_GQR0 + 4:
	case SPR_GQR0 + 5:
	case SPR_GQR0 + 6:
	case SPR_GQR0 + 7:
		// These are safe to do the easy way, see the bottom of this function.
	break;
	case SPR_XER:
		// XXX:
	default:
		LLVM_FALLBACK_IF(true);
	}

	Value* reg_rd = func->LoadGPR(inst.RD);
	func->StoreSPR(reg_rd, iIndex * 4);
}

void JitLLVM::mfmsr(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);

	func->StoreGPR(func->LoadMSR(), inst.RD);
}

void JitLLVM::mcrf(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);

	func->StoreCR(func->LoadCR(inst.CRFS), inst.CRFD);
}

void JitLLVM::crXX(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);
	IRBuilder<>* builder = func->GetBuilder();

	_dbg_assert_msg_(DYNA_REC, inst.OPCD == 19, "Invalid crXXX");

	// Special case: crclr
	if (inst.CRBA == inst.CRBB && inst.CRBA == inst.CRBD && inst.SUBOP10 == 193)
	{
		ClearCRBit(func, inst.CRBD >> 2, 3 - (inst.CRBD & 3));
		return;
	}

	// Special case: crset
	if (inst.CRBA == inst.CRBB && inst.CRBA == inst.CRBD && inst.SUBOP10 == 289)
	{
		SetCRBit(func, inst.CRBD >> 2, 3 - (inst.CRBD & 3));
		return;
	}

	// creqv or crnand or crnor
	bool negateA = inst.SUBOP10 == 289 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;
	// crandc or crorc or crnand or crnor
	bool negateB = inst.SUBOP10 == 129 || inst.SUBOP10 == 417 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;

	Value* cr_a = GetCRBit(func, inst.CRBA >> 2, 3 - (inst.CRBA & 3), negateA);
	Value* cr_b = GetCRBit(func, inst.CRBB >> 2, 3 - (inst.CRBB & 3), negateB);
	Value* cr_res;

	// Compute combined bit
	switch (inst.SUBOP10)
	{
	case 33:  // crnor: ~(A || B) == (~A && ~B)
	case 129: // crandc: A && ~B
	case 257: // crand:  A && B
		cr_res = builder->CreateAnd(cr_a, cr_b);
		break;

	case 193: // crxor: A ^ B
	case 289: // creqv: ~(A ^ B) = ~A ^ B
		cr_res = builder->CreateXor(cr_a, cr_b);
		break;

	case 225: // crnand: ~(A && B) == (~A || ~B)
	case 417: // crorc: A || ~B
	case 449: // cror:  A || B
		cr_res = builder->CreateOr(cr_a, cr_b);
		break;
	default:
		cr_res = UndefValue::get(builder->getInt1Ty());
		break;
	}

	// Store result bit in CRBD
	SetCRBit(func, inst.CRBD >> 2, 3 - (inst.CRBD & 3), cr_res);
}

void JitLLVM::mfsr(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* res = func->LoadSR(builder->getInt32(inst.SR));
	res = builder->CreateZExt(res, builder->getInt32Ty());

	func->StoreGPR(res, inst.RD);
}

void JitLLVM::mfsrin(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);
	IRBuilder<>* builder = func->GetBuilder();

	int b = inst.RB, d = inst.RD;
	Value* reg_rb = func->LoadGPR(b);
	reg_rb = builder->CreateLShr(reg_rb, builder->getInt32(28));
	reg_rb = builder->CreateAnd(reg_rb, builder->getInt32(0xF));
	Value* res = func->LoadSR(reg_rb);
	res = builder->CreateZExt(res, builder->getInt32Ty());

	func->StoreGPR(res, d);
}

void JitLLVM::mtsr(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* reg_rs = func->LoadGPR(inst.RS);
	reg_rs = builder->CreateTrunc(reg_rs, builder->getInt8Ty());

	func->StoreSR(reg_rs, builder->getInt32(inst.SR));
}

void JitLLVM::mtsrin(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITSystemRegistersOff);
	IRBuilder<>* builder = func->GetBuilder();

	int b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);

	reg_rs = builder->CreateTrunc(reg_rs, builder->getInt8Ty());

	reg_rb = builder->CreateLShr(reg_rb, builder->getInt32(28));
	reg_rb = builder->CreateAnd(reg_rb, builder->getInt32(0xF));
	func->StoreSR(reg_rs, reg_rb);
}
