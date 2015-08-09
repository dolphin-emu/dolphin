// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <llvm/IR/Intrinsics.h>
#include "Core/PowerPC/JitLLVM/Jit.h"

using namespace llvm;

Value* JitLLVM::GetEA(LLVMFunction* func, UGeckoInstruction inst)
{
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA;
	if (a)
		return builder->CreateAdd(func->LoadGPR(a), builder->getInt32((s32)inst.SIMM_16));

	return builder->getInt32((s32)inst.SIMM_16);
}

Value* JitLLVM::GetEAU(LLVMFunction* func, UGeckoInstruction inst)
{
	IRBuilder<>* builder = func->GetBuilder();
	return builder->CreateAdd(func->LoadGPR(inst.RA), builder->getInt32((s32)inst.SIMM_16));
}

Value* JitLLVM::GetEAX(LLVMFunction* func, UGeckoInstruction inst)
{
	IRBuilder<>* builder = func->GetBuilder();

	int a = inst.RA, b = inst.RB;
	if (a)
		return builder->CreateAdd(func->LoadGPR(a), func->LoadGPR(b));

	return func->LoadGPR(b);
}

Value* JitLLVM::GetEAUX(LLVMFunction* func, UGeckoInstruction inst)
{
	IRBuilder<>* builder = func->GetBuilder();
	return builder->CreateAdd(func->LoadGPR(inst.RA), func->LoadGPR(inst.RB));
}

Value* JitLLVM::CreateReadU64(LLVMFunction* func, Value* address)
{
	return m_cur_mod->CreateReadU64(func, address);
}

Value* JitLLVM::CreateReadU32(LLVMFunction* func, Value* address)
{
	return m_cur_mod->CreateReadU32(func, address);
}

Value* JitLLVM::CreateReadU16(LLVMFunction* func, Value* address, bool needs_sext)
{
	return m_cur_mod->CreateReadU16(func, address, needs_sext);
}

Value* JitLLVM::CreateReadU8(LLVMFunction* func, Value* address, bool needs_sext)
{
	return m_cur_mod->CreateReadU8(func, address, needs_sext);
}

Value* JitLLVM::CreateWriteU64(LLVMFunction* func, Value* val, Value* address)
{
	return m_cur_mod->CreateWriteU64(func, val, address);
}

Value* JitLLVM::CreateWriteU32(LLVMFunction* func, Value* val, Value* address)
{
	return m_cur_mod->CreateWriteU32(func, val, address);
}

Value* JitLLVM::CreateWriteU16(LLVMFunction* func, Value* val, Value* address)
{
	return m_cur_mod->CreateWriteU16(func, val, address);
}

Value* JitLLVM::CreateWriteU8(LLVMFunction* func, Value* val, Value* address)
{
	return m_cur_mod->CreateWriteU8(func, val, address);
}

void JitLLVM::lwz(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEA(func, inst);
	Value* res = CreateReadU32(func, address);
	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lwzu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAU(func, inst);
	Value* res = CreateReadU32(func, address);
	func->StoreGPR(res, inst.RD);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lbz(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEA(func, inst);
	Value* res = CreateReadU8(func, address, false);
	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lbzu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAU(func, inst);
	Value* res = CreateReadU8(func, address, false);
	func->StoreGPR(res, inst.RD);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lhz(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEA(func, inst);
	Value* res = CreateReadU16(func, address, false);
	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lhzu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAU(func, inst);
	Value* res = CreateReadU16(func, address, false);
	func->StoreGPR(res, inst.RD);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lha(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEA(func, inst);
	Value* res = CreateReadU16(func, address, true);
	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lhau(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAU(func, inst);
	Value* res = CreateReadU16(func, address, true);
	func->StoreGPR(res, inst.RD);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stw(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEA(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU32(func, val, address);
}

void JitLLVM::stwu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAU(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU32(func, val, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::sth(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEA(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU16(func, val, address);
}

void JitLLVM::sthu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAU(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU16(func, val, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stb(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEA(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU8(func, val, address);
}

void JitLLVM::stbu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAU(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU8(func, val, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lmw(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEA(func, inst);
	for (int iReg = inst.RD; iReg <= 31; iReg++)
	{
		Value* res = CreateReadU32(func, address);
		func->StoreGPR(res, iReg);
		address = builder->CreateAdd(address, builder->getInt32(4));
	}
}

void JitLLVM::stmw(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEA(func, inst);
	for (int iReg = inst.RD; iReg <= 31; iReg++)
	{
		Value* res = func->LoadGPR(iReg);
		CreateWriteU32(func, res, address);
		address = builder->CreateAdd(address, builder->getInt32(4));
	}
}

void JitLLVM::lwzx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAX(func, inst);
	Value* res = CreateReadU32(func, address);
	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lwzux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAUX(func, inst);
	Value* res = CreateReadU32(func, address);
	func->StoreGPR(res, inst.RD);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lhzx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAX(func, inst);
	Value* res = CreateReadU16(func, address, false);
	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lhzux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAUX(func, inst);
	Value* res = CreateReadU16(func, address, false);
	func->StoreGPR(res, inst.RD);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lhax(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAX(func, inst);
	Value* res = CreateReadU16(func, address, true);
	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lhaux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAUX(func, inst);
	Value* res = CreateReadU16(func, address, true);
	func->StoreGPR(res, inst.RD);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lbzx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAX(func, inst);
	Value* res = CreateReadU8(func, address, false);
	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lbzux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAUX(func, inst);
	Value* res = CreateReadU8(func, address, false);
	func->StoreGPR(res, inst.RD);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lwbrx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* res = CreateReadU32(func, address);

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getInt32Ty());

	std::vector<Value *> args;
	args.push_back(res);

	Function* bswap_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::bswap, arg_type);
	res = builder->CreateCall(bswap_fun, args);

	func->StoreGPR(res, inst.RD);
}

void JitLLVM::lhbrx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* res = CreateReadU16(func, address, false);
	res = builder->CreateTrunc(res, builder->getInt16Ty());

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getInt16Ty());

	std::vector<Value *> args;
	args.push_back(res);

	Function* bswap_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::bswap, arg_type);
	res = builder->CreateCall(bswap_fun, args);
	res = builder->CreateZExt(res, builder->getInt32Ty());

	func->StoreGPR(res, inst.RD);
}

void JitLLVM::stwx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAX(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU32(func, val, address);
}

void JitLLVM::stwux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAUX(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU32(func, val, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::sthx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAX(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU16(func, val, address);
}

void JitLLVM::sthux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAUX(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU16(func, val, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stbx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAX(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU8(func, val, address);
}

void JitLLVM::stbux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);

	Value* address = GetEAUX(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	CreateWriteU8(func, val, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stwbrx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* val = func->LoadGPR(inst.RS);

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getInt32Ty());

	std::vector<Value *> args;
	args.push_back(val);

	Function* bswap_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::bswap, arg_type);
	val = builder->CreateCall(bswap_fun, args);

	CreateWriteU32(func, val, address);
}

void JitLLVM::sthbrx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* val = func->LoadGPR(inst.RS);
	val = builder->CreateTrunc(val, builder->getInt16Ty());

	std::vector<Type *> arg_type;
	arg_type.push_back(builder->getInt16Ty());

	std::vector<Value *> args;
	args.push_back(val);

	Function* bswap_fun = Intrinsic::getDeclaration(m_cur_mod->GetModule(), Intrinsic::bswap, arg_type);
	val = builder->CreateCall(bswap_fun, args);

	CreateWriteU16(func, val, address);
}
