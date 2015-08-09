// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitLLVM/Jit.h"

using namespace llvm;

void JitLLVM::lfs(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEA(func, inst);
	Value* res = CreateReadU32(func, address);
	res = builder->CreateBitCast(res, builder->getFloatTy());
	res = builder->CreateFPExt(res, builder->getDoubleTy());
	func->StoreFPR(res, inst.FD, false);
	func->StoreFPR(res, inst.FD, true);
}

void JitLLVM::lfsu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAU(func, inst);
	Value* res = CreateReadU32(func, address);
	res = builder->CreateBitCast(res, builder->getFloatTy());
	res = builder->CreateFPExt(res, builder->getDoubleTy());
	func->StoreFPR(res, inst.FD, false);
	func->StoreFPR(res, inst.FD, true);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lfd(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEA(func, inst);
	Value* res = CreateReadU64(func, address);
	res = builder->CreateBitCast(res, builder->getDoubleTy());
	func->StoreFPR(res, inst.FD, false);
}

void JitLLVM::lfdu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAU(func, inst);
	Value* res = CreateReadU64(func, address);
	res = builder->CreateBitCast(res, builder->getDoubleTy());
	func->StoreFPR(res, inst.FD, false);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stfs(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEA(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = ForceSingle(func, res);
	res = builder->CreateBitCast(res, builder->getInt32Ty());
	CreateWriteU32(func, res, address);
}

void JitLLVM::stfsu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAU(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = ForceSingle(func, res);
	res = builder->CreateBitCast(res, builder->getInt32Ty());
	CreateWriteU32(func, res, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stfd(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEA(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = builder->CreateBitCast(res, builder->getInt64Ty());
	CreateWriteU64(func, res, address);
}

void JitLLVM::stfdu(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEA(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = builder->CreateBitCast(res, builder->getInt64Ty());
	CreateWriteU64(func, res, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lfsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* res = CreateReadU32(func, address);
	res = builder->CreateBitCast(res, builder->getFloatTy());
	res = builder->CreateFPExt(res, builder->getDoubleTy());
	func->StoreFPR(res, inst.FD, false);
	func->StoreFPR(res, inst.FD, true);
}

void JitLLVM::lfsux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAUX(func, inst);
	Value* res = CreateReadU32(func, address);
	res = builder->CreateBitCast(res, builder->getFloatTy());
	res = builder->CreateFPExt(res, builder->getDoubleTy());
	func->StoreFPR(res, inst.FD, false);
	func->StoreFPR(res, inst.FD, true);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::lfdx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* res = CreateReadU64(func, address);
	res = builder->CreateBitCast(res, builder->getDoubleTy());
	func->StoreFPR(res, inst.FD, false);
}

void JitLLVM::lfdux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAUX(func, inst);
	Value* res = CreateReadU64(func, address);
	res = builder->CreateBitCast(res, builder->getDoubleTy());
	func->StoreFPR(res, inst.FD, false);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stfsx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = ForceSingle(func, res);
	res = builder->CreateBitCast(res, builder->getInt32Ty());
	CreateWriteU32(func, res, address);
}

void JitLLVM::stfsux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAUX(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = ForceSingle(func, res);
	res = builder->CreateBitCast(res, builder->getInt32Ty());
	CreateWriteU32(func, res, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stfdx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = builder->CreateBitCast(res, builder->getInt64Ty());
	CreateWriteU64(func, res, address);
}

void JitLLVM::stfdux(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = builder->CreateBitCast(res, builder->getInt64Ty());
	CreateWriteU64(func, res, address);
	func->StoreGPR(address, inst.RA);
}

void JitLLVM::stfiwx(LLVMFunction* func, UGeckoInstruction inst)
{
	INSTRUCTION_START
	LLVM_JITDISABLE(bJITLoadStoreOff);
	IRBuilder<>* builder = func->GetBuilder();

	Value* address = GetEAX(func, inst);
	Value* res = func->LoadFPR(inst.FS, false);
	res = builder->CreateBitCast(res, builder->getInt64Ty());
	res = builder->CreateTrunc(res, builder->getInt32Ty());
	CreateWriteU32(func, res, address);
}
