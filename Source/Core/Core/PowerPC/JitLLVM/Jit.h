// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitLLVM/JitBinding.h"
#include "Core/PowerPC/JitLLVM/JitFunction.h"
#include "Core/PowerPC/JitLLVM/JitModule.h"

class LLVMNamedBinding;
class LLVMBinding;
class LLVMFunction;
class LLVMMemoryManager;
class LLVMModule;

#define LLVM_FALLBACK_IF(cond) do { if (cond) { FallBackToInterpreter(func, inst); return; } } while (0)

#define LLVM_JITDISABLE(setting) LLVM_FALLBACK_IF(SConfig::GetInstance().bJITOff || \
                                        SConfig::GetInstance().setting)

class JitLLVM : public JitBase
{
private:
	std::unordered_map<u32, LLVMModule*> m_mods;
	llvm::Module* m_main_mod;
	llvm::ExecutionEngine* m_engine;
	LLVMMemoryManager* m_mem_manager;
	LLVMNamedBinding* m_named_binder;

	// Debug helper
	bool m_debug_enabled;

	// Currently emitting things
	LLVMModule* m_cur_mod;
	LLVMFunction* m_cur_func;
	LLVMBinding* m_cur_binder;

	LLVMFunction* GetCurrentFunc() { return m_cur_func; }

	PPCAnalyst::CodeBuffer code_buffer;

	llvm::Value* JumpIfCRFieldBit(LLVMFunction* func, int field, int bit, bool jump_if_set);
	void reg_imm(LLVMFunction* func, u32 d, u32 a, bool binary, u32 value, llvm::Instruction::BinaryOps opc, bool Rc = false);

	// Helpers
	llvm::Value* ForceSingle(LLVMFunction* func, llvm::Value* val);
	// Returns an ICmpGT value
	llvm::Value* HasCarry(LLVMFunction* func, llvm::Value* a, llvm::Value* b);
	void ComputeRC(LLVMFunction* func, llvm::Value* val, int crf, bool needs_sext = true);
	llvm::Value* GetCRBit(LLVMFunction* func, int field, int bit, bool negate);
	void SetCRBit(LLVMFunction* func, int field, int bit);
	void SetCRBit(LLVMFunction* func, int field, int bit, llvm::Value* val);
	void ClearCRBit(LLVMFunction* func, int field, int bit);

	// LoadStore Helpers
	llvm::Value* GetEA(LLVMFunction* func, UGeckoInstruction inst);
	llvm::Value* GetEAU(LLVMFunction* func, UGeckoInstruction inst);
	llvm::Value* GetEAX(LLVMFunction* func, UGeckoInstruction inst);
	llvm::Value* GetEAUX(LLVMFunction* func, UGeckoInstruction inst);
	llvm::Value* CreateReadU64(LLVMFunction* func, llvm::Value* address);
	llvm::Value* CreateReadU32(LLVMFunction* func, llvm::Value* address);
	llvm::Value* CreateReadU16(LLVMFunction* func, llvm::Value* address, bool needs_sext);
	llvm::Value* CreateReadU8(LLVMFunction* func, llvm::Value* address, bool needs_sext);
	llvm::Value* CreateWriteU64(LLVMFunction* func, llvm::Value* val, llvm::Value* address);
	llvm::Value* CreateWriteU32(LLVMFunction* func, llvm::Value* val, llvm::Value* address);
	llvm::Value* CreateWriteU16(LLVMFunction* func, llvm::Value* val, llvm::Value* address);
	llvm::Value* CreateWriteU8(LLVMFunction* func, llvm::Value* val, llvm::Value* address);

	// Paired Loadstore helpers
	llvm::Value* Paired_GetEA(LLVMFunction* func, UGeckoInstruction inst);
	llvm::Value* Paired_GetEAU(LLVMFunction* func, UGeckoInstruction inst);
	llvm::Value* Paired_GetEAX(LLVMFunction* func, UGeckoInstruction inst);
	llvm::Value* Paired_GetEAUX(LLVMFunction* func, UGeckoInstruction inst);
	llvm::Value* CreatePairedLoad(LLVMFunction* func, llvm::Value* paired, llvm::Value* type, llvm::Value* scale, llvm::Value* address);
	llvm::Value* Paired_GetLoadType(LLVMFunction* func, llvm::Value* gqr);
	llvm::Value* Paired_GetLoadScale(LLVMFunction* func, llvm::Value* gqr);

public:
	JitLLVM() : code_buffer(32000) {}

	void Init() override;
	void Shutdown() override;

	// Can't actually clear the cache yet
	void ClearCache() override {}
	void SingleStep() override {}

	void Run() override {};

	const char* GetName() override { return "LLVMJIT"; }

	// XXX: Tie our LLVM memory manager and block cache together?
	JitBaseBlockCache* GetBlockCache() override { return nullptr; }

	void Jit(u32 em_address) override;

	// We don't generate our own ASM routines
	// We ride on the ASM routines that the tier 1 recompiler uses
	const CommonAsmRoutinesBase* GetAsmRoutines() override { return nullptr; }

	// We don't actually handle faults...yet
	bool HandleFault(uintptr_t access_address, SContext* ctx) override { return false; }

	void DoDownCount();
	void WriteExit(u32 destination);
	void WriteExitInValue(llvm::Value* destination);
	void WriteExceptionExit(llvm::Value* val);

	// Opcode
	void FallBackToInterpreter(LLVMFunction* func, UGeckoInstruction inst);
	void DoNothing(LLVMFunction* func, UGeckoInstruction inst);

	void DynaRunTable4 (LLVMFunction* func, UGeckoInstruction inst);
	void DynaRunTable19(LLVMFunction* func, UGeckoInstruction inst);
	void DynaRunTable31(LLVMFunction* func, UGeckoInstruction inst);
	void DynaRunTable59(LLVMFunction* func, UGeckoInstruction inst);
	void DynaRunTable63(LLVMFunction* func, UGeckoInstruction inst);

	// Force break
	void Break(LLVMFunction* func, UGeckoInstruction inst);

	// Branches
	void bx(LLVMFunction* func, UGeckoInstruction inst);
	void bcx(LLVMFunction* func, UGeckoInstruction inst);
	void bcctrx(LLVMFunction* func, UGeckoInstruction inst);
	void bclrx(LLVMFunction* func, UGeckoInstruction inst);
	void sc(LLVMFunction* func, UGeckoInstruction inst);
	void rfi(LLVMFunction* func, UGeckoInstruction inst);
	void mtmsr(LLVMFunction* func, UGeckoInstruction inst);
	void icbi(LLVMFunction* func, UGeckoInstruction inst);

	// Integer
	void arith_imm(LLVMFunction* func, UGeckoInstruction inst);
	void rlwXX(LLVMFunction* func, UGeckoInstruction inst);
	void rlwinmx(LLVMFunction* func, UGeckoInstruction inst);
	void rlwnmx(LLVMFunction* func, UGeckoInstruction inst);
	void mulli(LLVMFunction* func, UGeckoInstruction inst);
	void mullwx(LLVMFunction* func, UGeckoInstruction inst);
	void mulhwx(LLVMFunction* func, UGeckoInstruction inst);
	void mulhwux(LLVMFunction* func, UGeckoInstruction inst);
	void negx(LLVMFunction* func, UGeckoInstruction inst);
	void subfx(LLVMFunction* func, UGeckoInstruction inst);
	void xorx(LLVMFunction* func, UGeckoInstruction inst);
	void orx(LLVMFunction* func, UGeckoInstruction inst);
	void orcx(LLVMFunction* func, UGeckoInstruction inst);
	void norx(LLVMFunction* func, UGeckoInstruction inst);
	void nandx(LLVMFunction* func, UGeckoInstruction inst);
	void eqvx(LLVMFunction* func, UGeckoInstruction inst);
	void andx(LLVMFunction* func, UGeckoInstruction inst);
	void andcx(LLVMFunction* func, UGeckoInstruction inst);
	void slwx(LLVMFunction* func, UGeckoInstruction inst);
	void srwx(LLVMFunction* func, UGeckoInstruction inst);
	void addx(LLVMFunction* func, UGeckoInstruction inst);
	void cmp(LLVMFunction* func, UGeckoInstruction inst);
	void cmpi(LLVMFunction* func, UGeckoInstruction inst);
	void cmpl(LLVMFunction* func, UGeckoInstruction inst);
	void cmpli(LLVMFunction* func, UGeckoInstruction inst);
	void extsXx(LLVMFunction* func, UGeckoInstruction inst);
	void cntlzwx(LLVMFunction* func, UGeckoInstruction inst);
	void addic(LLVMFunction* func, UGeckoInstruction inst);
	void addic_rc(LLVMFunction* func, UGeckoInstruction inst);
	void addcx(LLVMFunction* func, UGeckoInstruction inst);
	void addex(LLVMFunction* func, UGeckoInstruction inst);
	void addmex(LLVMFunction* func, UGeckoInstruction inst);
	void addzex(LLVMFunction* func, UGeckoInstruction inst);
	void subfcx(LLVMFunction* func, UGeckoInstruction inst);
	void subfex(LLVMFunction* func, UGeckoInstruction inst);
	void subfmex(LLVMFunction* func, UGeckoInstruction inst);
	void subfzex(LLVMFunction* func, UGeckoInstruction inst);
	void subfic(LLVMFunction* func, UGeckoInstruction inst);

	// System Registers
	void mtspr(LLVMFunction* func, UGeckoInstruction inst);
	void mftb(LLVMFunction* func, UGeckoInstruction inst);
	void mfspr(LLVMFunction* func, UGeckoInstruction inst);
	void mfmsr(LLVMFunction* func, UGeckoInstruction inst);
	void mcrf(LLVMFunction* func, UGeckoInstruction inst);
	void crXX(LLVMFunction* func, UGeckoInstruction inst);
	void mfsr(LLVMFunction* func, UGeckoInstruction inst);
	void mfsrin(LLVMFunction* func, UGeckoInstruction inst);
	void mtsr(LLVMFunction* func, UGeckoInstruction inst);
	void mtsrin(LLVMFunction* func, UGeckoInstruction inst);

	// Floating Point
	void fabsx(LLVMFunction* func, UGeckoInstruction inst);
	void faddsx(LLVMFunction* func, UGeckoInstruction inst);
	void faddx(LLVMFunction* func, UGeckoInstruction inst);
	void fmaddsx(LLVMFunction* func, UGeckoInstruction inst);
	void fmaddx(LLVMFunction* func, UGeckoInstruction inst);
	void fmrx(LLVMFunction* func, UGeckoInstruction inst);
	void fmsubsx(LLVMFunction* func, UGeckoInstruction inst);
	void fmsubx(LLVMFunction* func, UGeckoInstruction inst);
	void fmulsx(LLVMFunction* func, UGeckoInstruction inst);
	void fmulx(LLVMFunction* func, UGeckoInstruction inst);
	void fnabsx(LLVMFunction* func, UGeckoInstruction inst);
	void fnegx(LLVMFunction* func, UGeckoInstruction inst);
	void fnmaddsx(LLVMFunction* func, UGeckoInstruction inst);
	void fnmaddx(LLVMFunction* func, UGeckoInstruction inst);
	void fnmsubsx(LLVMFunction* func, UGeckoInstruction inst);
	void fnmsubx(LLVMFunction* func, UGeckoInstruction inst);
	void fselx(LLVMFunction* func, UGeckoInstruction inst);
	void fsubsx(LLVMFunction* func, UGeckoInstruction inst);
	void fsubx(LLVMFunction* func, UGeckoInstruction inst);

	// Paired
	void ps_abs(LLVMFunction* func, UGeckoInstruction inst);
	void ps_add(LLVMFunction* func, UGeckoInstruction inst);
	void ps_sub(LLVMFunction* func, UGeckoInstruction inst);
	void ps_div(LLVMFunction* func, UGeckoInstruction inst);
	void ps_madd(LLVMFunction* func, UGeckoInstruction inst);
	void ps_madds0(LLVMFunction* func, UGeckoInstruction inst);
	void ps_madds1(LLVMFunction* func, UGeckoInstruction inst);
	void ps_merge00(LLVMFunction* func, UGeckoInstruction inst);
	void ps_merge01(LLVMFunction* func, UGeckoInstruction inst);
	void ps_merge10(LLVMFunction* func, UGeckoInstruction inst);
	void ps_merge11(LLVMFunction* func, UGeckoInstruction inst);
	void ps_mr(LLVMFunction* func, UGeckoInstruction inst);
	void ps_mul(LLVMFunction* func, UGeckoInstruction inst);
	void ps_muls0(LLVMFunction* func, UGeckoInstruction inst);
	void ps_muls1(LLVMFunction* func, UGeckoInstruction inst);
	void ps_msub(LLVMFunction* func, UGeckoInstruction inst);
	void ps_nabs(LLVMFunction* func, UGeckoInstruction inst);
	void ps_neg(LLVMFunction* func, UGeckoInstruction inst);
	void ps_nmadd(LLVMFunction* func, UGeckoInstruction inst);
	void ps_nmsub(LLVMFunction* func, UGeckoInstruction inst);
	void ps_sum0(LLVMFunction* func, UGeckoInstruction inst);
	void ps_sum1(LLVMFunction* func, UGeckoInstruction inst);
	void ps_res(LLVMFunction* func, UGeckoInstruction inst);
	void ps_sel(LLVMFunction* func, UGeckoInstruction inst);

	// LoadStore
	void lwz(LLVMFunction* func, UGeckoInstruction inst);
	void lwzu(LLVMFunction* func, UGeckoInstruction inst);
	void lhz(LLVMFunction* func, UGeckoInstruction inst);
	void lhzu(LLVMFunction* func, UGeckoInstruction inst);
	void lbz(LLVMFunction* func, UGeckoInstruction inst);
	void lbzu(LLVMFunction* func, UGeckoInstruction inst);
	void lha(LLVMFunction* func, UGeckoInstruction inst);
	void lhau(LLVMFunction* func, UGeckoInstruction inst);
	void stw(LLVMFunction* func, UGeckoInstruction inst);
	void stwu(LLVMFunction* func, UGeckoInstruction inst);
	void sth(LLVMFunction* func, UGeckoInstruction inst);
	void sthu(LLVMFunction* func, UGeckoInstruction inst);
	void stb(LLVMFunction* func, UGeckoInstruction inst);
	void stbu(LLVMFunction* func, UGeckoInstruction inst);
	void lmw(LLVMFunction* func, UGeckoInstruction inst);
	void stmw(LLVMFunction* func, UGeckoInstruction inst);
	void lwzx(LLVMFunction* func, UGeckoInstruction inst);
	void lwzux(LLVMFunction* func, UGeckoInstruction inst);
	void lhzx(LLVMFunction* func, UGeckoInstruction inst);
	void lhzux(LLVMFunction* func, UGeckoInstruction inst);
	void lhax(LLVMFunction* func, UGeckoInstruction inst);
	void lhaux(LLVMFunction* func, UGeckoInstruction inst);
	void lbzx(LLVMFunction* func, UGeckoInstruction inst);
	void lbzux(LLVMFunction* func, UGeckoInstruction inst);
	void lwbrx(LLVMFunction* func, UGeckoInstruction inst);
	void lhbrx(LLVMFunction* func, UGeckoInstruction inst);
	void stwx(LLVMFunction* func, UGeckoInstruction inst);
	void stwux(LLVMFunction* func, UGeckoInstruction inst);
	void sthx(LLVMFunction* func, UGeckoInstruction inst);
	void sthux(LLVMFunction* func, UGeckoInstruction inst);
	void stbx(LLVMFunction* func, UGeckoInstruction inst);
	void stbux(LLVMFunction* func, UGeckoInstruction inst);
	void stwbrx(LLVMFunction* func, UGeckoInstruction inst);
	void sthbrx(LLVMFunction* func, UGeckoInstruction inst);
	void stfiwx(LLVMFunction* func, UGeckoInstruction inst);

	// LoadStore Floating
	void lfs(LLVMFunction* func, UGeckoInstruction inst);
	void lfsu(LLVMFunction* func, UGeckoInstruction inst);
	void lfd(LLVMFunction* func, UGeckoInstruction inst);
	void lfdu(LLVMFunction* func, UGeckoInstruction inst);
	void stfs(LLVMFunction* func, UGeckoInstruction inst);
	void stfsu(LLVMFunction* func, UGeckoInstruction inst);
	void stfd(LLVMFunction* func, UGeckoInstruction inst);
	void stfdu(LLVMFunction* func, UGeckoInstruction inst);
	void lfsx(LLVMFunction* func, UGeckoInstruction inst);
	void lfsux(LLVMFunction* func, UGeckoInstruction inst);
	void lfdx(LLVMFunction* func, UGeckoInstruction inst);
	void lfdux(LLVMFunction* func, UGeckoInstruction inst);
	void stfsx(LLVMFunction* func, UGeckoInstruction inst);
	void stfsux(LLVMFunction* func, UGeckoInstruction inst);
	void stfdx(LLVMFunction* func, UGeckoInstruction inst);
	void stfdux(LLVMFunction* func, UGeckoInstruction inst);

	// LoadStore Paired
	void psq_lXX(LLVMFunction* func, UGeckoInstruction inst);
};
