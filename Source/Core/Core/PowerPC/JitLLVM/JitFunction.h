// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitLLVM/Jit.h"
enum
{
	INDEX_GPR,
	INDEX_PC,
	INDEX_NPC,
	INDEX_CR,
	INDEX_MSR,
	INDEX_FPSCR,
	INDEX_EXCEPTIONS,
	INDEX_DOWNCOUNT,
	INDEX_XER_CA,
	INDEX_XER_SO_OV,
	INDEX_XER_STRCTL,
	INDEX_PS,
	INDEX_SR,
	INDEX_SPR,
};


class LLVMFunction
{
private:
	llvm::Module* m_mod;
	llvm::Function* m_func;
	llvm::IRBuilder<> m_builder;
	u32 m_address;

	// Helper values
	struct
	{
		llvm::LoadInst* m_ppcstate;
		llvm::Value* m_PCGEP;
		llvm::Value* m_NPCGEP;
		llvm::Value* m_CRGEP[8];
		llvm::Value* m_MSRGEP;
		llvm::Value* m_EXCGEP;
		llvm::Value* m_CTRGEP;
		llvm::Value* m_LRGEP;
	} m_ptrs;

	llvm::Value* GetPCGEP();
	llvm::Value* GetNPCGEP();
	llvm::Value* GetCRGEP(int field);
	llvm::Value* GetMSRGEP();
	llvm::Value* GetEXCGEP();
	llvm::Value* GetCTRGEP();
	llvm::Value* GetLRGEP();

public:

	LLVMFunction(llvm::Module* mod, u32 address, llvm::FunctionType* type);
	LLVMFunction(llvm::Module* mod, const std::string& name, llvm::FunctionType* type);

	llvm::Function* GetFunction() { return m_func; }
	llvm::IRBuilder<>* GetBuilder() { return &m_builder; }

	void BuildGEPs();

	// Helpers
	llvm::LoadInst* GetPPCState();
	llvm::LoadInst* LoadPC();
	llvm::LoadInst* LoadNPC();
	llvm::LoadInst* LoadCTR();
	llvm::LoadInst* LoadMSR();
	llvm::LoadInst* LoadExceptions();
	llvm::LoadInst* LoadCR(int field);
	llvm::LoadInst* LoadLR();
	llvm::LoadInst* LoadSPR(int spr);
	llvm::Value*    LoadCarry();
	llvm::LoadInst* LoadSR(llvm::Value* sr);
	llvm::LoadInst* LoadGQR(int gqr);
	llvm::LoadInst* LoadDownCount();
	void StorePC(llvm::Value* val);
	void StoreNPC(llvm::Value* val);
	void StoreCTR(llvm::Value* val);
	void StoreCR(llvm::Value* val, int field);
	void StoreLR(llvm::Value* val);
	void StoreMSR(llvm::Value* val);
	void StoreExceptions(llvm::Value* val);
	void StoreSPR(llvm::Value* val, int spr);
	void StoreCarry(llvm::Value* val);
	void StoreSR(llvm::Value* val, llvm::Value* sr);
	void StoreDownCount(llvm::Value* val);
	llvm::LoadInst* LoadGPR(int reg);
	void StoreGPR(llvm::Value* val, int reg);
	llvm::LoadInst* LoadFPR(int reg, bool ps1);
	void StoreFPR(llvm::Value* val, int reg, bool ps1);

	llvm::BasicBlock* CreateBlock(const std::string& name);
	void AddBlock(llvm::BasicBlock* block);
};
