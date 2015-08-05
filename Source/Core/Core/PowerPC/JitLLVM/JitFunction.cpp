// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitLLVM/JitFunction.h"

using namespace llvm;

LLVMFunction::LLVMFunction(Module* mod, u32 address, FunctionType* type)
	: m_builder(mod->getContext())
{
	m_mod = mod;
	m_address = address;

	//
	memset(&m_ptrs, 0, sizeof(m_ptrs));

	std::string func_name = StringFromFormat("ppc%08x", address);

	LLVMContext& con = getGlobalContext();

	// Generate the function
	m_func = Function::Create(
		type,
		Function::ExternalLinkage,
		func_name,
		m_mod);
	m_func->setCallingConv(CallingConv::C);

	BasicBlock* block = BasicBlock::Create(con, "entry", m_func);
	m_builder.SetInsertPoint(block);
}

LLVMFunction::LLVMFunction(Module* mod, const std::string& name, FunctionType* type)
	: m_builder(mod->getContext())
{
	m_mod = mod;

	//
	memset(&m_ptrs, 0, sizeof(m_ptrs));

	LLVMContext& con = getGlobalContext();

	// Generate the function
	m_func = Function::Create(
		type,
		Function::ExternalLinkage,
		name,
		m_mod);
	m_func->setCallingConv(CallingConv::C);

	BasicBlock* block = BasicBlock::Create(con, "entry", m_func);
	m_builder.SetInsertPoint(block);
}

void LLVMFunction::BuildGEPs()
{
	// Build our GEPs
	GetPPCState();
	GetPCGEP();
	GetNPCGEP();
	for (int i = 0; i < 8; ++i)
		GetCRGEP(i);
	GetMSRGEP();
	GetEXCGEP();
	GetCTRGEP();
	GetLRGEP();
}

LoadInst* LLVMFunction::GetPPCState()
{
	if (!m_ptrs.m_ppcstate)
	{
		GlobalVariable* ppcStatePtr_global = m_mod->getNamedGlobal("ppcState_ptr");
		m_ptrs.m_ppcstate = m_builder.CreateLoad(ppcStatePtr_global, m_builder.getInt32(0), "ppcState");
	}

	return m_ptrs.m_ppcstate;
}

Value* LLVMFunction::GetPCGEP()
{
	if (!m_ptrs.m_PCGEP)
	{
		Value* ppcState = GetPPCState();

		const std::vector<Value*> pc_loc =
		{
			m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
			m_builder.getInt32(INDEX_PC), // 1th index in to index in structs(PC)
		};

		m_ptrs.m_PCGEP = m_builder.CreateGEP(ppcState, pc_loc, "PC");
	}

	return m_ptrs.m_PCGEP;
}

Value* LLVMFunction::GetNPCGEP()
{
	if (!m_ptrs.m_NPCGEP)
	{
		Value* ppcState = GetPPCState();

		const std::vector<Value*> ctr_loc =
		{
			m_builder.getInt32(0), // 0th index in to the PowerNPCState "array"
			m_builder.getInt32(INDEX_NPC), // 1th index in to index in structs(NPC)
		};

		m_ptrs.m_NPCGEP = m_builder.CreateGEP(ppcState, ctr_loc, "NPC");
	}

	return m_ptrs.m_NPCGEP;
}

Value* LLVMFunction::GetCTRGEP()
{
	if (!m_ptrs.m_CTRGEP)
	{
		Value* ppcState = GetPPCState();

		const std::vector<Value*> ctr_loc =
		{
			m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
			m_builder.getInt32(INDEX_SPR), // 1th index in to index in structs(PC)
			m_builder.getInt32(SPR_CTR),
		};

		m_ptrs.m_CTRGEP = m_builder.CreateGEP(ppcState, ctr_loc, "CTR");
	}

	return m_ptrs.m_CTRGEP;
}

Value* LLVMFunction::GetMSRGEP()
{
	if (!m_ptrs.m_MSRGEP)
	{
		Value* ppcState = GetPPCState();

		const std::vector<Value*> ctr_loc =
		{
			m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
			m_builder.getInt32(INDEX_MSR), // 1th index in to index in structs(PC)
		};

		m_ptrs.m_MSRGEP = m_builder.CreateGEP(ppcState, ctr_loc, "MSR");
	}

	return m_ptrs.m_MSRGEP;
}

Value* LLVMFunction::GetEXCGEP()
{
	if (!m_ptrs.m_EXCGEP)
	{
		Value* ppcState = GetPPCState();

		const std::vector<Value*> exc_loc =
		{
			m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
			m_builder.getInt32(INDEX_EXCEPTIONS),
		};

		m_ptrs.m_EXCGEP = m_builder.CreateGEP(ppcState, exc_loc, "Exceptions");
	}

	return m_ptrs.m_EXCGEP;
}

Value* LLVMFunction::GetCRGEP(int field)
{
	if (!m_ptrs.m_CRGEP[field])
	{
		Value* ppcState = GetPPCState();

		const std::vector<Value*> cr_loc =
		{
			m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
			m_builder.getInt32(INDEX_CR), // 1th index in to index in structs(PC)
			m_builder.getInt32(field),
		};

		m_ptrs.m_CRGEP[field] = m_builder.CreateGEP(ppcState, cr_loc, StringFromFormat("CR%d", field));
	}

	return m_ptrs.m_CRGEP[field];
}

Value* LLVMFunction::GetLRGEP()
{
	if (!m_ptrs.m_LRGEP)
	{
		Value* ppcState = GetPPCState();

		const std::vector<Value*> lr_loc =
		{
			m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
			m_builder.getInt32(INDEX_SPR), // 1th index in to index in structs(PC)
			m_builder.getInt32(SPR_LR),
		};

		m_ptrs.m_LRGEP = m_builder.CreateGEP(ppcState, lr_loc, "LR");
	}

	return m_ptrs.m_LRGEP;
}

LoadInst* LLVMFunction::LoadPC()
{
	return m_builder.CreateLoad(GetPCGEP());
}

LoadInst* LLVMFunction::LoadNPC()
{
	return m_builder.CreateLoad(GetNPCGEP());
}

LoadInst* LLVMFunction::LoadCTR()
{
	return m_builder.CreateLoad(GetCTRGEP());
}

LoadInst* LLVMFunction::LoadMSR()
{
	return m_builder.CreateLoad(GetMSRGEP());
}

LoadInst* LLVMFunction::LoadExceptions()
{
	return m_builder.CreateLoad(GetEXCGEP());
}

LoadInst* LLVMFunction::LoadCR(int field)
{
	return m_builder.CreateLoad(GetCRGEP(field));
}

LoadInst* LLVMFunction::LoadLR()
{
	return m_builder.CreateLoad(GetLRGEP());
}

LoadInst* LLVMFunction::LoadSPR(int spr)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> spr_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_SPR), // 0th index in to index in structs(SPRs)
		m_builder.getInt32(spr),
	};

	Value* spr_val = m_builder.CreateGEP(ppcState, spr_loc);
	return m_builder.CreateLoad(spr_val, StringFromFormat("SPR_%d", spr));
}

Value* LLVMFunction::LoadCarry()
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_XER_CA),
	};

	Value* val = m_builder.CreateGEP(ppcState, loc);
	Value* carry = m_builder.CreateLoad(val, "Carry");
	return m_builder.CreateTrunc(carry, m_builder.getInt1Ty());
}

LoadInst* LLVMFunction::LoadSR(Value* sr)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_SR),
		sr
	};

	Value* sr_val = m_builder.CreateGEP(ppcState, loc);
	return m_builder.CreateLoad(sr_val);
}

LoadInst* LLVMFunction::LoadGQR(int gqr)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> spr_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_SPR), // 0th index in to index in structs(SPRs)
		m_builder.getInt32(SPR_GQR0 + gqr),
	};

	Value* spr_val = m_builder.CreateGEP(ppcState, spr_loc);
	return m_builder.CreateLoad(spr_val, StringFromFormat("GQR_%d", gqr));
}

LoadInst* LLVMFunction::LoadDownCount()
{
	Value* ppcState = GetPPCState();

	std::vector<Value*> downcount_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_DOWNCOUNT), // 7th index in to index in structs(downcount)
	};

	Value* downcount = m_builder.CreateGEP(ppcState, downcount_loc, "Downcount");
	return m_builder.CreateLoad(downcount);
}

void LLVMFunction::StorePC(Value* val)
{
	m_builder.CreateStore(val, GetPCGEP());
}

void LLVMFunction::StoreNPC(Value* val)
{
	m_builder.CreateStore(val, GetNPCGEP());
}

void LLVMFunction::StoreCTR(Value* val)
{
	m_builder.CreateStore(val, GetCTRGEP());
}

void LLVMFunction::StoreMSR(Value* val)
{
	m_builder.CreateStore(val, GetMSRGEP());
}

void LLVMFunction::StoreExceptions(Value* val)
{
	m_builder.CreateStore(val, GetEXCGEP());
}

void LLVMFunction::StoreCR(Value* val, int field)
{
	m_builder.CreateStore(val, GetCRGEP(field));
}

void LLVMFunction::StoreLR(Value* val)
{
	m_builder.CreateStore(val, GetLRGEP());
}

LoadInst* LLVMFunction::LoadGPR(int reg)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> gpr_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_GPR), // 0th index in to index in structs(GPRS)
		m_builder.getInt32(reg),
	};

	Value* gpr = m_builder.CreateGEP(ppcState, gpr_loc);
	return m_builder.CreateLoad(gpr, StringFromFormat("GPR_%d", reg));
}

LoadInst* LLVMFunction::LoadFPR(int reg, bool ps1)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> fpr_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_PS),
		m_builder.getInt32(reg),
		m_builder.getInt32(ps1),
	};

	Value* fpr = m_builder.CreateGEP(ppcState, fpr_loc);
	return m_builder.CreateLoad(fpr, StringFromFormat("FPR_%d_%d", reg, ps1));
}

void LLVMFunction::StoreSPR(Value* val, int spr)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> spr_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_SPR), // 0th index in to index in structs(SPRs)
		m_builder.getInt32(spr),
	};

	Value* spr_gep = m_builder.CreateGEP(ppcState, spr_loc);
	m_builder.CreateStore(val, spr_gep);
}

void LLVMFunction::StoreCarry(Value* val)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_XER_CA),
	};

	Value* gep = m_builder.CreateGEP(ppcState, loc);
	Value* res = m_builder.CreateZExt(val, m_builder.getInt8Ty());
	m_builder.CreateStore(res, gep);
}

void LLVMFunction::StoreSR(Value* val, Value* sr)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_SR),
		sr,
	};

	Value* sr_val = m_builder.CreateGEP(ppcState, loc);
	m_builder.CreateStore(val, sr_val);
}

void LLVMFunction::StoreDownCount(Value* val)
{
	Value* ppcState = GetPPCState();

	std::vector<Value*> downcount_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_DOWNCOUNT), // 7th index in to index in structs(downcount)
	};

	Value* downcount = m_builder.CreateGEP(ppcState, downcount_loc, "Downcount");
	m_builder.CreateStore(val, downcount);
}

void LLVMFunction::StoreGPR(Value* val, int reg)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> gpr_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_GPR), // 0th index in to index in structs(GPRS)
		m_builder.getInt32(reg),
	};

	Value* gpr = m_builder.CreateGEP(ppcState, gpr_loc);
	m_builder.CreateStore(val, gpr);
}

void LLVMFunction::StoreFPR(Value* val, int reg, bool ps1)
{
	Value* ppcState = GetPPCState();

	const std::vector<Value*> fpr_loc =
	{
		m_builder.getInt32(0), // 0th index in to the PowerPCState "array"
		m_builder.getInt32(INDEX_PS),
		m_builder.getInt32(reg),
		m_builder.getInt32(ps1),
	};

	Value* fpr = m_builder.CreateGEP(ppcState, fpr_loc);
	val = m_builder.CreateFPExt(val, m_builder.getDoubleTy());
	m_builder.CreateStore(val, fpr);
}

BasicBlock* LLVMFunction::CreateBlock(const std::string& name)
{
	return BasicBlock::Create(m_mod->getContext(), name, m_func, nullptr);
}

void LLVMFunction::AddBlock(BasicBlock* block)
{
	m_func->getBasicBlockList().push_back(block);
}

