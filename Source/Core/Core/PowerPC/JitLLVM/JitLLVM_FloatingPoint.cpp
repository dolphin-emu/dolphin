// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <llvm/IR/Intrinsics.h>
#include "Core/PowerPC/JitLLVM/Jit.h"

using namespace llvm;

Value* JitLLVM::ForceSingle(LLVMFunction* func, Value* val)
{
	IRBuilder<>* builder = func->GetBuilder();

	return builder->CreateFPTrunc(val, builder->getFloatTy());
}

void JitLLVM::fabsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 b = inst.FB, d = inst.FD;

	Value* reg_fb = func->LoadFPR(b, false);

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getDoubleTy());

	std::vector<Value *> args;
	args.push_back(reg_fb);

	Function *fabs_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::fabs, arg_type);
	Value* res = builder->CreateCall(fabs_fun, args);

	func->StoreFPR(res, d, false);
}

void JitLLVM::faddsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);

	Value* res = builder->CreateFAdd(reg_fa, reg_fb);
	res = ForceSingle(func, res);

	func->StoreFPR(res, d, false);
	func->StoreFPR(res, d, true);
}

void JitLLVM::faddx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);

	Value* res = builder->CreateFAdd(reg_fa, reg_fb);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fmaddsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* mul_res = builder->CreateFMul(reg_fa, reg_fc);

	Value* res = builder->CreateFAdd(mul_res, reg_fb);
	res = ForceSingle(func, res);

	func->StoreFPR(res, d, false);
	func->StoreFPR(res, d, true);
}

void JitLLVM::fmaddx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* mul_res = builder->CreateFMul(reg_fa, reg_fc);

	Value* res = builder->CreateFAdd(mul_res, reg_fb);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fmrx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	func->StoreFPR(func->LoadFPR(b, false), d, false);
}

void JitLLVM::fmsubsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* mul_res = builder->CreateFMul(reg_fa, reg_fc);

	Value* res = builder->CreateFSub(mul_res, reg_fb);
	res = ForceSingle(func, res);

	func->StoreFPR(res, d, false);
	func->StoreFPR(res, d, true);
}

void JitLLVM::fmsubx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* mul_res = builder->CreateFMul(reg_fa, reg_fc);

	Value* res = builder->CreateFSub(mul_res, reg_fb);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fmulsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* res = builder->CreateFMul(reg_fa, reg_fc);
	res = ForceSingle(func, res);

	func->StoreFPR(res, d, false);
	func->StoreFPR(res, d, true);
}

void JitLLVM::fmulx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* res = builder->CreateFMul(reg_fa, reg_fc);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fnabsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 b = inst.FB, d = inst.FD;

	Value* reg_fb = func->LoadFPR(b, false);

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getDoubleTy());

	std::vector<Value *> args;
	args.push_back(reg_fb);

	Function *fabs_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::fabs, arg_type);
	Value* res = builder->CreateCall(fabs_fun, args);
	res = builder->CreateFNeg(res);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fnegx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 b = inst.FB, d = inst.FD;

	Value* reg_fb = func->LoadFPR(b, false);

	Value* res = builder->CreateFNeg(reg_fb);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fnmaddsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* mul_res = builder->CreateFMul(reg_fa, reg_fc);

	Value* res = builder->CreateFAdd(mul_res, reg_fb);
	res = builder->CreateFNeg(reg_fb);
	res = ForceSingle(func, res);

	func->StoreFPR(res, d, false);
	func->StoreFPR(res, d, true);
}

void JitLLVM::fnmaddx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* mul_res = builder->CreateFMul(reg_fa, reg_fc);

	Value* res = builder->CreateFAdd(mul_res, reg_fb);
	res = builder->CreateFNeg(reg_fb);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fnmsubsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* mul_res = builder->CreateFMul(reg_fa, reg_fc);

	Value* res = builder->CreateFSub(mul_res, reg_fb);
	res = builder->CreateFNeg(reg_fb);
	res = ForceSingle(func, res);

	func->StoreFPR(res, d, false);
	func->StoreFPR(res, d, true);
}

void JitLLVM::fnmsubx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* mul_res = builder->CreateFMul(reg_fa, reg_fc);

	Value* res = builder->CreateFSub(mul_res, reg_fb);
	res = builder->CreateFNeg(reg_fb);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fsubsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);

	Value* res = builder->CreateFSub(reg_fa, reg_fb);
	res = ForceSingle(func, res);

	func->StoreFPR(res, d, false);
	func->StoreFPR(res, d, true);
}

void JitLLVM::fsubx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);

	Value* res = builder->CreateFSub(reg_fa, reg_fb);

	func->StoreFPR(res, d, false);
}

void JitLLVM::fselx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* zero = builder->CreateUIToFP(builder->getInt32(0), builder->getDoubleTy());

	Value* reg_fa = func->LoadFPR(a, false);
	Value* reg_fb = func->LoadFPR(b, false);
	Value* reg_fc = func->LoadFPR(c, false);

	Value* cmp_res = builder->CreateFCmpUGE(reg_fa, zero);
	Value* res = builder->CreateSelect(cmp_res, reg_fc, reg_fb);

	func->StoreFPR(res, d, false);
}
