// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitLLVM/Jit.h"

using namespace llvm;

Value* JitLLVM::Paired_GetEA(LLVMFunction* func, UGeckoInstruction inst)
{
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA;
	if (a)
		return builder->CreateAdd(func->LoadGPR(a), builder->getInt32((s32)inst.SIMM_12));

	return builder->getInt32((s32)inst.SIMM_12);
}

Value* JitLLVM::Paired_GetEAU(LLVMFunction* func, UGeckoInstruction inst)
{
	IRBuilder<>* builder = func->GetBuilder();
	return builder->CreateAdd(func->LoadGPR(inst.RA), builder->getInt32((s32)inst.SIMM_12));
}

Value* JitLLVM::Paired_GetEAX(LLVMFunction* func, UGeckoInstruction inst)
{
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB;
	if (a)
		return builder->CreateAdd(func->LoadGPR(a), func->LoadGPR(b));

	return func->LoadGPR(b);
}

Value* JitLLVM::Paired_GetEAUX(LLVMFunction* func, UGeckoInstruction inst)
{
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB;
	return builder->CreateAdd(func->LoadGPR(a), func->LoadGPR(b));
}

Value* JitLLVM::CreatePairedLoad(LLVMFunction* func, Value* paired, Value* type, Value* scale, Value* address)
{
	return m_cur_mod->CreatePairedLoad(func, paired, type, scale, address);
}

Value* JitLLVM::Paired_GetLoadType(LLVMFunction* func, Value* gqr)
{
	IRBuilder<>* builder = func->GetBuilder();
	Value* res = builder->CreateLShr(gqr, builder->getInt32(16));
	res = builder->CreateAnd(res, builder->getInt32(7));
	return builder->CreateTrunc(res, builder->getInt8Ty());
}

Value* JitLLVM::Paired_GetLoadScale(LLVMFunction* func, Value* gqr)
{
	IRBuilder<>* builder = func->GetBuilder();
	Value* res = builder->CreateLShr(gqr, builder->getInt32(24));
	res = builder->CreateAnd(res, builder->getInt32(0x3F));
	return builder->CreateTrunc(res, builder->getInt8Ty());
}

void JitLLVM::psq_lXX(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	s32 offset = inst.SIMM_12;
	bool indexed = inst.OPCD == 4;
	bool update = (inst.OPCD == 57 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));

	IRBuilder<>* builder = func->GetBuilder();

	Value* address;

	Value* gqr = func->LoadGQR(inst.I);
	Value* paired = builder->getInt8(inst.W ? 0 : 1);
	Value* type = Paired_GetLoadType(func, gqr);
	Value* scale = Paired_GetLoadScale(func, gqr);

	if (indexed)
		if (update)
			address = Paired_GetEAUX(func, inst);
		else
			address = Paired_GetEAX(func, inst);
	else
		if (update)
			address = Paired_GetEAU(func, inst);
		else
			address = Paired_GetEA(func, inst);

	Value* res = CreatePairedLoad(func, paired, type, scale, address);
	Value* PS0 = builder->CreateExtractElement(res, builder->getInt32(0));
	Value* PS1 = builder->CreateExtractElement(res, builder->getInt32(1));

	func->StoreFPR(PS0, inst.RD, false);
	func->StoreFPR(PS1, inst.RD, true);

	if (update)
		func->StoreGPR(address, inst.RA);
}

