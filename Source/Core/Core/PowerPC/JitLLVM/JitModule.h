// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitLLVM/Jit.h"
#include "Core/PowerPC/JitLLVM/JitBinding.h"
#include "Core/PowerPC/JitLLVM/JitFunction.h"

class LLVMNamedBinding;
class LLVMBinding;
class LLVMFunction;

class LLVMModule
{
private:
	llvm::Module* m_mod;
	llvm::ExecutionEngine* m_engine;
	u32 m_address;

	LLVMFunction* m_func;

	// Helpers
	LLVMFunction* m_paired_load;

	LLVMNamedBinding* m_named_binder;
	LLVMBinding* m_binder;

	void GenerateGlobalVariables();
	void BindGlobalFunctions();

	void GeneratePairedLoadFunction();

	llvm::Value* LoadDequantizeValue(LLVMFunction* func, llvm::Value* scale);

public:
	LLVMModule(llvm::ExecutionEngine* engine, u32 address);
	llvm::Module* GetModule() { return m_mod; }
	LLVMFunction* GetFunction() { return m_func; }
	LLVMBinding* GetBinding() { return m_binder; }
	LLVMNamedBinding* GetNamedBinding() { return m_named_binder; }

	// Helpers
	llvm::Value* CreateReadU64(LLVMFunction* func, llvm::Value* address);
	llvm::Value* CreateReadU32(LLVMFunction* func, llvm::Value* address);
	llvm::Value* CreateReadU16(LLVMFunction* func, llvm::Value* address, bool needs_sext);
	llvm::Value* CreateReadU8(LLVMFunction* func, llvm::Value* address, bool needs_sext);
	llvm::Value* CreateWriteU64(LLVMFunction* func, llvm::Value* val, llvm::Value* address);
	llvm::Value* CreateWriteU32(LLVMFunction* func, llvm::Value* val, llvm::Value* address);
	llvm::Value* CreateWriteU16(LLVMFunction* func, llvm::Value* val, llvm::Value* address);
	llvm::Value* CreateWriteU8(LLVMFunction* func, llvm::Value* val, llvm::Value* address);

	// Returns a <2 x float> with the values of the registers that go in to PS{0,1}
	llvm::Value* CreatePairedLoad(LLVMFunction* func, llvm::Value* paired, llvm::Value* type, llvm::Value* scale, llvm::Value* address);

};
