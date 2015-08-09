// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <llvm/IR/Intrinsics.h>
#include "Core/PowerPC/JitLLVM/Jit.h"

using namespace llvm;

void JitLLVM::ps_abs(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 b = inst.FB, d = inst.FD;

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getDoubleTy());

	std::vector<Value *> args;
	args.push_back(reg_fb0);

	Function *fabs_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::fabs, arg_type);
	Value* res0 = builder->CreateCall(fabs_fun, args);

	args[0] = reg_fb1;
	Value* res1 = builder->CreateCall(fabs_fun, args);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_add(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* res0 = builder->CreateFAdd(reg_fa0, reg_fb0);
	Value* res1 = builder->CreateFAdd(reg_fa1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_sub(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* res0 = builder->CreateFSub(reg_fa0, reg_fb0);
	Value* res1 = builder->CreateFSub(reg_fa1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_div(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* res0 = builder->CreateFDiv(reg_fa0, reg_fb0);
	Value* res1 = builder->CreateFDiv(reg_fa1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_madd(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc0 = func->LoadFPR(c, false);
	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* mul_res0 = builder->CreateFMul(reg_fa0, reg_fc0);
	Value* mul_res1 = builder->CreateFMul(reg_fa1, reg_fc1);

	Value* res0 = builder->CreateFAdd(mul_res0, reg_fb0);
	Value* res1 = builder->CreateFAdd(mul_res1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_madds0(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc0 = func->LoadFPR(c, false);

	Value* mul_res0 = builder->CreateFMul(reg_fa0, reg_fc0);
	Value* mul_res1 = builder->CreateFMul(reg_fa1, reg_fc0);

	Value* res0 = builder->CreateFAdd(mul_res0, reg_fb0);
	Value* res1 = builder->CreateFAdd(mul_res1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_madds1(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* mul_res0 = builder->CreateFMul(reg_fa0, reg_fc1);
	Value* mul_res1 = builder->CreateFMul(reg_fa1, reg_fc1);

	Value* res0 = builder->CreateFAdd(mul_res0, reg_fb0);
	Value* res1 = builder->CreateFAdd(mul_res1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_merge00(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);

	Value* reg_fb0 = func->LoadFPR(b, false);

	func->StoreFPR(reg_fa0, d, false);
	func->StoreFPR(reg_fb0, d, true);
}

void JitLLVM::ps_merge01(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);

	Value* reg_fb1 = func->LoadFPR(b, true);

	func->StoreFPR(reg_fa0, d, false);
	func->StoreFPR(reg_fb1, d, true);
}

void JitLLVM::ps_merge10(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);

	func->StoreFPR(reg_fa1, d, false);
	func->StoreFPR(reg_fb0, d, true);
}

void JitLLVM::ps_merge11(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);

	u32 a = inst.FA, b = inst.FB, d = inst.FD;

	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb1 = func->LoadFPR(b, true);

	func->StoreFPR(reg_fa1, d, false);
	func->StoreFPR(reg_fb1, d, true);
}

void JitLLVM::ps_mr(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);

	u32 b = inst.FB, d = inst.FD;

	if (b == d)
		return;

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	func->StoreFPR(reg_fb0, d, false);
	func->StoreFPR(reg_fb1, d, true);
}

void JitLLVM::ps_mul(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fc0 = func->LoadFPR(c, false);
	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* res0 = builder->CreateFMul(reg_fa0, reg_fc0);
	Value* res1 = builder->CreateFMul(reg_fa1, reg_fc1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_muls0(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fc0 = func->LoadFPR(c, false);

	Value* res0 = builder->CreateFMul(reg_fa0, reg_fc0);
	Value* res1 = builder->CreateFMul(reg_fa1, reg_fc0);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_muls1(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* res0 = builder->CreateFMul(reg_fa0, reg_fc1);
	Value* res1 = builder->CreateFMul(reg_fa1, reg_fc1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_msub(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc0 = func->LoadFPR(c, false);
	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* mul_res0 = builder->CreateFMul(reg_fa0, reg_fc0);
	Value* mul_res1 = builder->CreateFMul(reg_fa1, reg_fc1);

	Value* res0 = builder->CreateFSub(mul_res0, reg_fb0);
	Value* res1 = builder->CreateFSub(mul_res1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_nabs(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 b = inst.FB, d = inst.FD;

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getDoubleTy());

	std::vector<Value *> args;
	args.push_back(reg_fb0);

	Function *fabs_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::fabs, arg_type);
	Value* res0 = builder->CreateCall(fabs_fun, args);
	res0 = builder->CreateFNeg(res0);

	args[0] = reg_fb1;
	Value* res1 = builder->CreateCall(fabs_fun, args);
	res1 = builder->CreateFNeg(res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_neg(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 b = inst.FB, d = inst.FD;

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* res0 = builder->CreateFNeg(reg_fb0);
	Value* res1 = builder->CreateFNeg(reg_fb1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_nmadd(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc0 = func->LoadFPR(c, false);
	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* mul_res0 = builder->CreateFMul(reg_fa0, reg_fc0);
	Value* mul_res1 = builder->CreateFMul(reg_fa1, reg_fc1);

	Value* res0 = builder->CreateFAdd(mul_res0, reg_fb0);
	Value* res1 = builder->CreateFAdd(mul_res1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	res0 = builder->CreateFNeg(res0);
	res1 = builder->CreateFNeg(res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_nmsub(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc0 = func->LoadFPR(c, false);
	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* mul_res0 = builder->CreateFMul(reg_fa0, reg_fc0);
	Value* mul_res1 = builder->CreateFMul(reg_fa1, reg_fc1);

	Value* res0 = builder->CreateFSub(mul_res0, reg_fb0);
	Value* res1 = builder->CreateFSub(mul_res1, reg_fb1);

	res0 = ForceSingle(func, res0);
	res1 = ForceSingle(func, res1);

	res0 = builder->CreateFNeg(res0);
	res1 = builder->CreateFNeg(res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_sum0(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);

	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* res0 = builder->CreateFAdd(reg_fa0, reg_fb1);

	res0 = ForceSingle(func, res0);
	Value* res1 = ForceSingle(func, reg_fc1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_sum1(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Value* reg_fa0 = func->LoadFPR(a, false);

	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc0 = func->LoadFPR(c, false);

	Value* res0 = ForceSingle(func, reg_fc0);

	Value* res1 = builder->CreateFAdd(reg_fa0, reg_fb1);

	res1 = ForceSingle(func, res1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_res(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 b = inst.FB, d = inst.FD;

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getDoubleTy());

	std::vector<Value *> args;
	args.push_back(reg_fb0);

	Function *fabs_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::sqrt, arg_type);
	Value* res0 = builder->CreateCall(fabs_fun, args);

	args[0] = reg_fb1;
	Value* res1 = builder->CreateCall(fabs_fun, args);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}

void JitLLVM::ps_sel(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITFloatingPointOff);
	LLVM_FALLBACK_IF(inst.Rc);
	IRBuilder<>* builder = func->GetBuilder();

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	Constant* zero = ConstantFP::get(builder->getDoubleTy(), 0.0);

	Value* reg_fa0 = func->LoadFPR(a, false);
	Value* reg_fa1 = func->LoadFPR(a, true);

	Value* reg_fb0 = func->LoadFPR(b, false);
	Value* reg_fb1 = func->LoadFPR(b, true);

	Value* reg_fc0 = func->LoadFPR(c, false);
	Value* reg_fc1 = func->LoadFPR(c, true);

	Value* cmp_res0 = builder->CreateFCmpUGE(reg_fa0, zero);
	Value* res0 = builder->CreateSelect(cmp_res0, reg_fc0, reg_fb0);

	Value* cmp_res1 = builder->CreateFCmpUGE(reg_fa1, zero);
	Value* res1 = builder->CreateSelect(cmp_res1, reg_fc1, reg_fb1);

	func->StoreFPR(res0, d, false);
	func->StoreFPR(res1, d, true);
}
