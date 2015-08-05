// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <llvm/IR/Intrinsics.h>
#include "Core/PowerPC/JitLLVM/Jit.h"

using namespace llvm;

void JitLLVM::ComputeRC(LLVMFunction* func, Value* val, int crf, bool needs_sext)
{
	LLVMContext& con = getGlobalContext();
	IRBuilder<>* builder = func->GetBuilder();
	Type* llvm_i64 = Type::getInt64Ty(con);

	if (needs_sext)
	{
		Value* res = builder->CreateSExtOrTrunc(val, llvm_i64);
		func->StoreCR(res, crf);
	}
	else
	{
		Value* res = builder->CreateZExtOrTrunc(val, llvm_i64);
		func->StoreCR(res, crf);
	}
}

Value* JitLLVM::HasCarry(LLVMFunction* func, Value* a, Value* b)
{
	IRBuilder<>* builder = func->GetBuilder();

	b = builder->CreateNot(b);
	return builder->CreateICmpUGT(a, b);
}

void JitLLVM::reg_imm(LLVMFunction* func, u32 d, u32 a, bool binary, u32 value, Instruction::BinaryOps opc, bool Rc)
{
	IRBuilder<>* builder = func->GetBuilder();

	if (a || binary)
	{
		Value* reg_ra = func->LoadGPR(a);
		Value* res = builder->CreateBinOp(opc, reg_ra, builder->getInt32(value));
		func->StoreGPR(res, d);

		if (Rc)
			ComputeRC(func, res, 0);
	}
	else if (opc == Instruction::BinaryOps::Add)
	{
		// a == 0, implies zero register
		Value* res = builder->getInt32(value);
		func->StoreGPR(res, d);
		if (Rc)
			ComputeRC(func, res, 0);
	}
	else
	{
		_assert_msg_(DYNA_REC, false, "Hit impossible condition in reg_imm!");
	}
}

void JitLLVM::arith_imm(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	u32 d = inst.RD, a = inst.RA, s = inst.RS;

	switch (inst.OPCD)
	{
	case 14: // addi
		reg_imm(func, d, a, false, (u32)(s32)inst.SIMM_16, Instruction::BinaryOps::Add);
	break;
	case 15: // addis
		reg_imm(func, d, a, false, (u32)inst.SIMM_16 << 16, Instruction::BinaryOps::Add);
	break;
	case 24: // ori
		if (a == 0 && s == 0 && inst.UIMM == 0 && !inst.Rc)  //check for nop
		{
			// NOP
			return;
		}
		reg_imm(func, a, s, true, inst.UIMM,       Instruction::BinaryOps::Or);
	break;
	case 25: // oris
		reg_imm(func, a, s, true, inst.UIMM << 16, Instruction::BinaryOps::Or);
	break;
	case 28: // andi
		reg_imm(func, a, s, true, inst.UIMM,       Instruction::BinaryOps::And, true);
	break;
	case 29: // andis
		reg_imm(func, a, s, true, inst.UIMM << 16, Instruction::BinaryOps::And, true);
	break;
	case 26: // xori
		reg_imm(func, a, s, true, inst.UIMM,       Instruction::BinaryOps::Xor);
	break;
	case 27: // xoris
		reg_imm(func, a, s, true, inst.UIMM << 16, Instruction::BinaryOps::Xor);
	break;
	}
}

void JitLLVM::rlwXX(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.RA, s = inst.RS;
	u32 mask_val = Helper_Mask(inst.MB, inst.ME);
	Value* lsl_amount, *lsr_amount;
	Value* mask, *neg_mask;
	Value* res;

	Value* reg_rs = func->LoadGPR(s);

	if (inst.OPCD == 23) // rlwnmx
	{
		Value* reg_rb = func->LoadGPR(inst.RB);
		lsl_amount = builder->CreateAnd(reg_rb, builder->getInt32(0x1F));
		lsr_amount = builder->CreateSub(builder->getInt32(32), lsl_amount);
	}
	else
	{
		lsl_amount = builder->getInt32(inst.SH);
		lsr_amount = builder->getInt32((32 - inst.SH) & 0x1F);
	}

	mask = builder->getInt32(mask_val);
	neg_mask = builder->getInt32(~mask_val);

	Value* lsl_res = builder->CreateShl(reg_rs, lsl_amount);
	Value* lsr_res = builder->CreateLShr(reg_rs, lsr_amount);
	Value* rotl_res = builder->CreateOr(lsl_res, lsr_res);

	if (inst.OPCD == 20) // rlwimix
	{
		Value* reg_ra = func->LoadGPR(a);
		Value* masked_ra = builder->CreateAnd(reg_ra, neg_mask);
		Value* masked_rs = builder->CreateAnd(rotl_res, mask);
		res = builder->CreateOr(masked_ra, masked_rs);
	}
	else // rlwinmx/rlwnmx
	{
		res = builder->CreateAnd(rotl_res, mask);
	}

	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::mulli(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* rhs_val = builder->getInt32(inst.SIMM_16);
	Value* res = builder->CreateMul(reg_ra, rhs_val);
	func->StoreGPR(res, d);
}

void JitLLVM::mullwx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	LLVM_FALLBACK_IF(inst.OE);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* res = builder->CreateMul(reg_ra, reg_rb);
	func->StoreGPR(res, d);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::mulhwx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	LLVMContext& con = getGlobalContext();
	IRBuilder<>* builder = func->GetBuilder();
	Type* llvm_i32 = Type::getInt32Ty(con);
	Type* llvm_i64 = Type::getInt64Ty(con);

	int a = inst.RA, b = inst.RB, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* ra64 = builder->CreateSExtOrTrunc(reg_ra, llvm_i64);
	Value* rb64 = builder->CreateSExtOrTrunc(reg_rb, llvm_i64);
	Value* res64 = builder->CreateMul(ra64, rb64);
	Value* res = builder->CreateLShr(res64, builder->getInt64(32));
	res = builder->CreateZExtOrTrunc(res, llvm_i32);

	func->StoreGPR(res, d);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::mulhwux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	LLVMContext& con = getGlobalContext();
	IRBuilder<>* builder = func->GetBuilder();
	Type* llvm_i32 = Type::getInt32Ty(con);
	Type* llvm_i64 = Type::getInt64Ty(con);

	int a = inst.RA, b = inst.RB, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* ra64 = builder->CreateZExtOrTrunc(reg_ra, llvm_i64);
	Value* rb64 = builder->CreateZExtOrTrunc(reg_rb, llvm_i64);
	Value* res64 = builder->CreateMul(ra64, rb64);
	Value* res = builder->CreateLShr(res64, builder->getInt64(32));
	res = builder->CreateZExtOrTrunc(res, llvm_i32);
	func->StoreGPR(res, d);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::negx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, d = inst.RD;
	Value* reg_ra = func->LoadGPR(a);
	Value* res = builder->CreateNeg(reg_ra);
	func->StoreGPR(res, d);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::subfx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, d = inst.RD;
	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* res = builder->CreateSub(reg_rb, reg_ra);
	func->StoreGPR(res, d);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::xorx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* res = builder->CreateXor(reg_rs, reg_rb);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::orx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* res = builder->CreateOr(reg_rs, reg_rb);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::orcx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* res = builder->CreateOr(reg_rs, builder->CreateNot(reg_rb));
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::norx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* res = builder->CreateOr(reg_rs, reg_rb);
	res = builder->CreateNot(res);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::nandx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* res = builder->CreateAnd(reg_rs, reg_rb);
	res = builder->CreateNot(res);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::eqvx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* res = builder->CreateXor(reg_rs, reg_rb);
	res = builder->CreateNot(res);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::andx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* res = builder->CreateAnd(reg_rs, reg_rb);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::andcx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* res = builder->CreateAnd(reg_rs, builder->CreateNot(reg_rb));
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::slwx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* cmp_res = builder->CreateICmpEQ(reg_rb, builder->getInt32(0x20));
	Value* shift_amount = builder->CreateAnd(reg_rb, builder->getInt32(0x1F));
	Value* shifted_res = builder->CreateShl(reg_rs, shift_amount);
	Value* res = builder->CreateSelect(cmp_res, builder->getInt32(0), shifted_res);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::srwx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, s = inst.RS;
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_rs = func->LoadGPR(s);
	Value* cmp_res = builder->CreateICmpEQ(reg_rb, builder->getInt32(0x20));
	Value* shift_amount = builder->CreateAnd(reg_rb, builder->getInt32(0x1F));
	Value* shifted_res = builder->CreateLShr(reg_rs, shift_amount);
	Value* res = builder->CreateSelect(cmp_res, builder->getInt32(0), shifted_res);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::addx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, d = inst.RD;
	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* res = builder->CreateAdd(reg_rb, reg_ra);
	func->StoreGPR(res, d);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::cmp(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int crf = inst.CRFD;
	int a = inst.RA, b = inst.RB;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* res = builder->CreateSub(reg_ra, reg_rb);

	ComputeRC(func, res, crf);

}

void JitLLVM::cmpi(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int crf = inst.CRFD;
	int a = inst.RA;

	Value* imm_val = builder->getInt32(inst.UIMM);

	Value* reg_ra = func->LoadGPR(a);
	Value* res = builder->CreateSub(reg_ra, imm_val);

	ComputeRC(func, res, crf, false);
}

void JitLLVM::cmpl(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	LLVMContext& con = getGlobalContext();
	IRBuilder<>* builder = func->GetBuilder();
	Type* llvm_i64 = Type::getInt64Ty(con);

	int crf = inst.CRFD;
	int a = inst.RA, b = inst.RB;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* reg_xa = builder->CreateZExtOrTrunc(reg_ra, llvm_i64);
	Value* reg_xb = builder->CreateZExtOrTrunc(reg_rb, llvm_i64);
	Value* res = builder->CreateSub(reg_xa, reg_xb);

	ComputeRC(func, res, crf, false);
}

void JitLLVM::cmpli(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	LLVMContext& con = getGlobalContext();
	IRBuilder<>* builder = func->GetBuilder();
	Type* llvm_i64 = Type::getInt64Ty(con);

	int crf = inst.CRFD;
	int a = inst.RA;

	Value* imm_val = builder->getInt64(inst.UIMM);

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_xa = builder->CreateZExtOrTrunc(reg_ra, llvm_i64);
	Value* res = builder->CreateSub(reg_xa, imm_val);

	ComputeRC(func, res, crf, false);
}

void JitLLVM::extsXx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, s = inst.RS;
	int size = inst.SUBOP10 == 922 ? 16 : 8;

	Value* reg_rs = func->LoadGPR(s);
	Value* trunc_reg = builder->CreateTrunc(reg_rs, builder->getIntNTy(size));
	Value* res = builder->CreateSExt(trunc_reg, builder->getInt32Ty());
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::cntlzwx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, s = inst.RS;

	Value* reg_rs = func->LoadGPR(s);
	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getInt32Ty());

	std::vector<Value *> args;
	args.push_back(reg_rs);
	args.push_back(builder->getInt1(false));

	Function *ctlz_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::ctlz, arg_type);
	Value* res = builder->CreateCall(ctlz_fun, args);
	func->StoreGPR(res, a);

	if (inst.Rc)
		ComputeRC(func, res, 0);
}

void JitLLVM::addic(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* imm = builder->getInt32((u32)(s32)inst.SIMM_16);

	Value* res = builder->CreateAdd(reg_ra, imm);
	func->StoreGPR(res, d);

	Value* has_carry = HasCarry(func, reg_ra, imm);
	func->StoreCarry(has_carry);
}

void JitLLVM::addic_rc(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);

	addic(func, inst);
	ComputeRC(func, func->LoadGPR(inst.RD), 0);
}

void JitLLVM::addcx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);

	Value* res = builder->CreateAdd(reg_ra, reg_rb);
	func->StoreGPR(res, d);

	Value* has_carry = HasCarry(func, reg_ra, reg_rb);
	func->StoreCarry(has_carry);

	if (inst.OE)
		PanicAlert("OE: addcx");

	if (inst.Rc)
		ComputeRC(func, func->LoadGPR(d), 0);
}

void JitLLVM::addex(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* old_carry = func->LoadCarry();
	Value* old_carry_value = builder->CreateZExt(old_carry, builder->getInt32Ty());

	Value* res = builder->CreateAdd(reg_ra, reg_rb);
	res = builder->CreateAdd(res, old_carry_value);
	func->StoreGPR(res, d);

	// SetCarry(Helper_Carry(a, b) || (carry != 0 && Helper_Carry(a + b, carry)));
	Value* has_carry0 = HasCarry(func, reg_ra, reg_rb);
	Value* has_carry1 = HasCarry(func, res, old_carry_value);
	Value* carry_res = builder->CreateOr(has_carry0, has_carry1);

	func->StoreCarry(carry_res);

	if (inst.OE)
		PanicAlert("OE: addex");

	if (inst.Rc)
		ComputeRC(func, func->LoadGPR(d), 0);
}

void JitLLVM::addmex(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* old_carry = func->LoadCarry();
	Value* old_carry_value = builder->CreateZExt(old_carry, builder->getInt32Ty());

	Value* res = builder->CreateAdd(reg_ra, reg_rb);
	res = builder->CreateSub(res, old_carry_value);
	func->StoreGPR(res, d);

	// SetCarry(Helper_Carry(a, carry - 1));
	func->StoreCarry(HasCarry(func, reg_ra, builder->CreateSub(old_carry_value, builder->getInt32(1))));

	if (inst.OE)
		PanicAlert("OE: addex");

	if (inst.Rc)
		ComputeRC(func, func->LoadGPR(d), 0);
}

void JitLLVM::addzex(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* old_carry = func->LoadCarry();
	Value* old_carry_value = builder->CreateZExt(old_carry, builder->getInt32Ty());

	Value* res = builder->CreateAdd(reg_ra, old_carry_value);
	func->StoreGPR(res, d);

	// SetCarry(Helper_Carry(a, carry));
	func->StoreCarry(HasCarry(func, reg_ra, old_carry_value));

	if (inst.OE)
		PanicAlert("OE: addex");

	if (inst.Rc)
		ComputeRC(func, func->LoadGPR(d), 0);
}

void JitLLVM::subfcx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);

	Value* res = builder->CreateSub(reg_rb, reg_ra);
	func->StoreGPR(res, d);

	// SetCarry(a == 0 || Helper_Carry(b, 0-a));
	Value* has_carry0 = builder->CreateICmpEQ(reg_ra, builder->getInt32(0));
	Value* has_carry1 = HasCarry(func, reg_rb, builder->CreateNeg(reg_ra));
	Value* carry_res = builder->CreateOr(has_carry0, has_carry1);

	func->StoreCarry(carry_res);

	if (inst.OE)
		PanicAlert("OE: addex");

	if (inst.Rc)
		ComputeRC(func, func->LoadGPR(d), 0);
}

void JitLLVM::subfex(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* reg_rb = func->LoadGPR(b);
	Value* old_carry = func->LoadCarry();
	Value* old_carry_value = builder->CreateZExt(old_carry, builder->getInt32Ty());

	Value* not_ra = builder->CreateNot(reg_ra);

	Value* res = builder->CreateAdd(not_ra, reg_rb);
	res = builder->CreateAdd(res, old_carry_value);
	func->StoreGPR(res, d);

	// SetCarry(Helper_Carry(~a, b) || Helper_Carry((~a) + b, carry));
	Value* has_carry0 = HasCarry(func, not_ra, reg_rb);
	Value* has_carry1 = HasCarry(func, builder->CreateAdd(not_ra, reg_rb), old_carry_value);
	Value* carry_res = builder->CreateOr(has_carry0, has_carry1);

	func->StoreCarry(carry_res);

	if (inst.OE)
		PanicAlert("OE: addex");

	if (inst.Rc)
		ComputeRC(func, func->LoadGPR(d), 0);
}

void JitLLVM::subfmex(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* old_carry = func->LoadCarry();
	Value* old_carry_value = builder->CreateZExt(old_carry, builder->getInt32Ty());

	Value* not_ra = builder->CreateNot(reg_ra);

	Value* res = builder->CreateAdd(not_ra, old_carry_value);
	res = builder->CreateSub(res, builder->getInt32(1));
	func->StoreGPR(res, d);

	// SetCarry(Helper_Carry(~a, carry - 1));
	Value* has_carry = HasCarry(func, not_ra, builder->CreateSub(old_carry_value, builder->getInt32(1)));
	func->StoreCarry(has_carry);

	if (inst.OE)
		PanicAlert("OE: addex");

	if (inst.Rc)
		ComputeRC(func, func->LoadGPR(d), 0);
}

void JitLLVM::subfzex(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* old_carry = func->LoadCarry();
	Value* old_carry_value = builder->CreateZExt(old_carry, builder->getInt32Ty());

	Value* not_ra = builder->CreateNot(reg_ra);

	Value* res = builder->CreateAdd(not_ra, old_carry_value);
	func->StoreGPR(res, d);

	// SetCarry(Helper_Carry(~a, carry));
	Value* has_carry = HasCarry(func, not_ra, old_carry_value);
	func->StoreCarry(has_carry);

	if (inst.OE)
		PanicAlert("OE: addex");

	if (inst.Rc)
		ComputeRC(func, func->LoadGPR(d), 0);
}

void JitLLVM::subfic(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITIntegerOff);
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, d = inst.RD;

	Value* reg_ra = func->LoadGPR(a);
	Value* imm = builder->getInt32((u32)(s32)inst.SIMM_16);

	Value* res = builder->CreateAdd(imm, reg_ra);
	func->StoreGPR(res, d);

	// SetCarry((rGPR[_inst.RA] == 0) || (Helper_Carry(0 - rGPR[_inst.RA], immediate)));
	reg_ra = func->LoadGPR(a); // May have changed from the previous store
	Value* has_carry0 = builder->CreateICmpEQ(reg_ra, builder->getInt32(0));
	Value* has_carry1 = HasCarry(func, builder->CreateNeg(reg_ra), imm);
	Value* carry_res = builder->CreateOr(has_carry0, has_carry1);

	func->StoreCarry(carry_res);
}
