// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitLLVM/Jit.h"

using namespace llvm;

void JitLLVM::bx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START

	IRBuilder<>* builder = func->GetBuilder();

	u32 destination;
	if (inst.AA)
		destination = SignExt26(inst.LI << 2);
	else
		destination = js.compilerPC + SignExt26(inst.LI << 2);

	if (inst.LK)
	{
		Value* new_lr = builder->getInt32(js.compilerPC + 4);
		func->StoreLR(new_lr);
	}

	if (destination == js.compilerPC)
		js.downcountAmount += 8;

	WriteExit(destination);
}

Value* JitLLVM::JumpIfCRFieldBit(LLVMFunction* func, int field, int bit, bool jump_if_set)
{
	LLVMContext& con = getGlobalContext();
	IRBuilder<>* builder = func->GetBuilder();

	Value* cr_cmp, *tmp_val;
	Value* zero = builder->getInt64(0);
	LoadInst* cr_val = func->LoadCR(field);
	switch (bit)
	{
	case CR_SO_BIT:  // check bit 61 set
		tmp_val = builder->CreateAnd(cr_val, builder->getInt64(1ULL << 61));
		if (jump_if_set)
			cr_cmp = builder->CreateICmpNE(tmp_val, zero);
		else
			cr_cmp = builder->CreateICmpEQ(tmp_val, zero);
	break;
	case CR_EQ_BIT:  // check bits 31-0 == 0
		tmp_val = builder->CreateAnd(cr_val, builder->getInt64(0xFFFFFFFF));
		if (jump_if_set)
			cr_cmp = builder->CreateICmpEQ(tmp_val, zero);
		else
			cr_cmp = builder->CreateICmpNE(tmp_val, zero);
	break;
	case CR_GT_BIT:  // check val > 0
		if (jump_if_set)
			cr_cmp = builder->CreateICmpSGT(cr_val, zero);
		else
			cr_cmp = builder->CreateICmpSLE(cr_val, zero);
	break;
	case CR_LT_BIT:  // check bit 62 set
		tmp_val = builder->CreateAnd(cr_val, builder->getInt64(1ULL << 62));
		if (jump_if_set)
			cr_cmp = builder->CreateICmpNE(tmp_val, zero);
		else
			cr_cmp = builder->CreateICmpEQ(tmp_val, zero);
	break;
	default:
		_assert_msg_(DYNA_REC, false, "Invalid CR bit");
		cr_cmp = UndefValue::get(Type::getInt64Ty(con));
	}

	return cr_cmp;
}

void JitLLVM::bcx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START

	IRBuilder<>* builder = func->GetBuilder();

	BasicBlock* do_branch = func->CreateBlock("bcx_do_branch");
	BasicBlock* no_branch = js.isLastInstruction ? nullptr : func->CreateBlock("bcx_no_branch");
	BasicBlock* cr_branch = func->CreateBlock("bcx_cr_test");

	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
	{
		LoadInst* ctr = func->LoadCTR();
		Value* new_ctr = builder->CreateSub(ctr, builder->getInt32(1));
		func->StoreCTR(new_ctr);

		Value* ctr_cmp;
		if (inst.BO & BO_BRANCH_IF_CTR_0)
			ctr_cmp = builder->CreateICmpNE(new_ctr, builder->getInt32(0));
		else
			ctr_cmp = builder->CreateICmpEQ(new_ctr, builder->getInt32(0));

		builder->CreateCondBr(ctr_cmp, do_branch, cr_branch);
	}
	else
		builder->CreateBr(cr_branch);

	builder->SetInsertPoint(cr_branch);
	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
	{
		Value* cr_cmp = JumpIfCRFieldBit(func, inst.BI >> 2, 3 - (inst.BI & 3),
		                                 !(inst.BO_2 & BO_BRANCH_IF_TRUE));
		builder->CreateCondBr(cr_cmp, do_branch, no_branch);
	}
	else
		builder->CreateBr(do_branch);

	builder->SetInsertPoint(do_branch);
	{
		if (inst.LK)
		{
			Value* new_lr = builder->getInt32(js.compilerPC + 4);
			func->StoreLR(new_lr);
		}

		u32 destination;
		if (inst.AA)
			destination = SignExt16(inst.BD << 2);
		else
			destination = js.compilerPC + SignExt16(inst.BD << 2);

		WriteExit(destination);
	}

	builder->SetInsertPoint(no_branch);

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
		WriteExit(js.compilerPC + 4);
}

void JitLLVM::bcctrx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START

	IRBuilder<>* builder = func->GetBuilder();

	// bcctrx doesn't decrement and/or test CTR
	_dbg_assert_msg_(POWERPC, inst.BO_2 & BO_DONT_DECREMENT_FLAG, "bcctrx with decrement and test CTR option is invalid!");
	if (inst.BO_2 & BO_DONT_CHECK_CONDITION)
	{
		Value* ctr = func->LoadSPR(SPR_CTR);
		if (inst.LK_3)
			func->StoreLR(builder->getInt32(js.compilerPC + 4));
		ctr = builder->CreateAnd(ctr, builder->getInt32(~3));
		WriteExitInValue(ctr);
	}
	else
	{
		BasicBlock* do_branch = func->CreateBlock("bcctrx_do_branch");
		BasicBlock* no_branch = js.isLastInstruction ? nullptr : func->CreateBlock("bcctrx_no_branch");

		Value* cr_cmp = JumpIfCRFieldBit(func, inst.BI >> 2, 3 - (inst.BI & 3),
		                                 !(inst.BO_2 & BO_BRANCH_IF_TRUE));
		builder->CreateCondBr(cr_cmp, do_branch, no_branch);

		builder->SetInsertPoint(do_branch);
		{
			Value* ctr = func->LoadSPR(SPR_CTR);
			if (inst.LK_3)
				func->StoreLR(builder->getInt32(js.compilerPC + 4));
			ctr = builder->CreateAnd(ctr, builder->getInt32(~3));
			WriteExitInValue(ctr);
		}

		builder->SetInsertPoint(no_branch);

		if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
			WriteExit(js.compilerPC + 4);
	}
}
void JitLLVM::bclrx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START

	IRBuilder<>* builder = func->GetBuilder();

	BasicBlock* do_branch = func->CreateBlock("bclrx_do_branch");
	BasicBlock* no_branch = js.isLastInstruction ? nullptr : func->CreateBlock("bclrx_no_branch");
	BasicBlock* cr_branch = func->CreateBlock("bclrx_cr_test");

	if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
	{
		LoadInst* ctr = func->LoadCTR();
		Value* new_ctr = builder->CreateSub(ctr, builder->getInt32(1));
		func->StoreCTR(new_ctr);

		Value* ctr_cmp;
		if (inst.BO & BO_BRANCH_IF_CTR_0)
			ctr_cmp = builder->CreateICmpNE(new_ctr, builder->getInt32(0));
		else
			ctr_cmp = builder->CreateICmpEQ(new_ctr, builder->getInt32(0));

		builder->CreateCondBr(ctr_cmp, do_branch, cr_branch);
	}
	else
		builder->CreateBr(cr_branch);

	builder->SetInsertPoint(cr_branch);
	if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
	{
		Value* cr_cmp = JumpIfCRFieldBit(func, inst.BI >> 2, 3 - (inst.BI & 3),
		                                 !(inst.BO_2 & BO_BRANCH_IF_TRUE));
		builder->CreateCondBr(cr_cmp, do_branch, no_branch);
	}
	else
		builder->CreateBr(do_branch);

	builder->SetInsertPoint(do_branch);
	{
		if (inst.LK)
		{
			Value* new_lr = builder->getInt32(js.compilerPC + 4);
			func->StoreLR(new_lr);
		}

		Value* destination = func->LoadLR();
		destination = builder->CreateAnd(destination, builder->getInt32(~3));
		WriteExitInValue(destination);
	}

	builder->SetInsertPoint(no_branch);

	if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
		WriteExit(js.compilerPC + 4);
}

void JitLLVM::sc(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START

	IRBuilder<>* builder = func->GetBuilder();
	Value* Exceptions = func->LoadExceptions();
	Exceptions = builder->CreateOr(Exceptions, builder->getInt32(EXCEPTION_SYSCALL));
	func->StoreExceptions(Exceptions);

	WriteExceptionExit(builder->getInt32(js.compilerPC + 4));
}

void JitLLVM::rfi(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START

	IRBuilder<>* builder = func->GetBuilder();

	// See Interpreter rfi for details
	Value* mask = builder->getInt32(0x87C0FFFF);
	Value* neg_mask = builder->getInt32(~0x87C0FFFF);
	Value* clearMSR13 = builder->getInt32(0xFFFBFFFF); // Mask used to clear the bit MSR[13]
	// MSR = ((MSR & ~mask) | (SRR1 & mask)) & clearMSR13;

	Value* msr = func->LoadMSR();

	msr = builder->CreateAnd(msr, neg_mask);

	Value* srr1 = func->LoadSPR(SPR_SRR1);

	srr1 = builder->CreateAnd(srr1, mask);

	msr = builder->CreateOr(msr, srr1);
	msr = builder->CreateAnd(msr, clearMSR13);

	func->StoreMSR(msr);

	Value* srr0 = func->LoadSPR(SPR_SRR0);
	WriteExceptionExit(srr0);
}

void JitLLVM::mtmsr(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START

	Value* reg_rs = func->LoadGPR(inst.RS);
	func->StoreMSR(reg_rs);
	WriteExit(js.compilerPC + 4);
}

void JitLLVM::icbi(LLVMFunction* func, UGeckoInstruction inst)
{
	FallBackToInterpreter(func, inst);
	WriteExit(js.compilerPC + 4);
}
