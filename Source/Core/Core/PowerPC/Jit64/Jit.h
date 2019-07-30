// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ========================
// See comments in Jit.cpp.
// ========================

// Mystery: Capcom vs SNK 800aa278

// CR flags approach:
//   * Store that "N+Z flag contains CR0" or "S+Z flag contains CR3".
//   * All flag altering instructions flush this
//   * A flush simply does a conditional write to the appropriate CRx.
//   * If flag available, branch code can become absolutely trivial.

// Settings
// ----------
#pragma once

#include "Common/CommonTypes.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/RegCache/FPURegCache.h"
#include "Core/PowerPC/Jit64/RegCache/GPRRegCache.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/BlockCache.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/Jit64Common/TrampolineCache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

namespace PPCAnalyst
{
struct CodeBlock;
struct CodeOp;
}  // namespace PPCAnalyst

class Jit64 : public JitBase, public QuantizedMemoryRoutines
{
public:
  Jit64();
  ~Jit64() override;

  void Init() override;
  void Shutdown() override;

  bool HandleFault(uintptr_t access_address, SContext* ctx) override;
  bool HandleStackFault() override;
  bool BackPatch(u32 emAddress, SContext* ctx);

  void EnableOptimization();
  void EnableBlockLink();

  // Jit!

  void Jit(u32 em_address) override;
  u8* DoJit(u32 em_address, JitBlock* b, u32 nextPC);

  BitSet32 CallerSavedRegistersInUse() const;
  BitSet8 ComputeStaticGQRs(const PPCAnalyst::CodeBlock&) const;

  void IntializeSpeculativeConstants();

  JitBlockCache* GetBlockCache() override { return &blocks; }
  void Trace();

  void ClearCache() override;

  const CommonAsmRoutines* GetAsmRoutines() override { return &asm_routines; }
  const char* GetName() const override { return "JIT64"; }
  // Run!
  void Run() override;
  void SingleStep() override;

  // Utilities for use by opcodes

  void FakeBLCall(u32 after);
  void WriteExit(u32 destination, bool bl = false, u32 after = 0);
  void JustWriteExit(u32 destination, bool bl, u32 after);
  void WriteExitDestInRSCRATCH(bool bl = false, u32 after = 0);
  void WriteBLRExit();
  void WriteExceptionExit();
  void WriteExternalExceptionExit();
  void WriteRfiExitDestInRSCRATCH();
  void WriteIdleExit(u32 destination);
  bool Cleanup();

  void GenerateConstantOverflow(bool overflow);
  void GenerateConstantOverflow(s64 val);
  void GenerateOverflow();
  void FinalizeCarryOverflow(bool oe, bool inv = false);
  void FinalizeCarry(Gen::CCFlags cond);
  void FinalizeCarry(bool ca);
  void ComputeRC(preg_t preg, bool needs_test = true, bool needs_sext = true);

  void AndWithMask(Gen::X64Reg reg, u32 mask);
  bool CheckMergedBranch(u32 crf) const;
  void DoMergedBranch();
  void DoMergedBranchCondition();
  void DoMergedBranchImmediate(s64 val);

  // Reads a given bit of a given CR register part.
  void GetCRFieldBit(int field, int bit, Gen::X64Reg out, bool negate = false);
  // Clobbers RDX.
  void SetCRFieldBit(int field, int bit, Gen::X64Reg in);
  void ClearCRFieldBit(int field, int bit);
  void SetCRFieldBit(int field, int bit);

  // Generates a branch that will check if a given bit of a CR register part
  // is set or not.
  Gen::FixupBranch JumpIfCRFieldBit(int field, int bit, bool jump_if_set = true);
  void SetFPRFIfNeeded(Gen::X64Reg xmm);

  void HandleNaNs(UGeckoInstruction inst, Gen::X64Reg xmm_out, Gen::X64Reg xmm_in,
                  Gen::X64Reg clobber = Gen::XMM0);

  void MultiplyImmediate(u32 imm, int a, int d, bool overflow);

  typedef u32 (*Operation)(u32 a, u32 b);
  void regimmop(int d, int a, bool binary, u32 value, Operation doop,
                void (Gen::XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&),
                bool Rc = false, bool carry = false);
  void FloatCompare(UGeckoInstruction inst, bool upper = false);
  void UpdateMXCSR();

  // OPCODES
  using Instruction = void (Jit64::*)(UGeckoInstruction instCode);
  void FallBackToInterpreter(UGeckoInstruction _inst);
  void DoNothing(UGeckoInstruction _inst);
  void HLEFunction(UGeckoInstruction _inst);

  void DynaRunTable4(UGeckoInstruction _inst);
  void DynaRunTable19(UGeckoInstruction _inst);
  void DynaRunTable31(UGeckoInstruction _inst);
  void DynaRunTable59(UGeckoInstruction _inst);
  void DynaRunTable63(UGeckoInstruction _inst);

  void addx(UGeckoInstruction inst);
  void arithcx(UGeckoInstruction inst);
  void mulli(UGeckoInstruction inst);
  void mulhwXx(UGeckoInstruction inst);
  void mullwx(UGeckoInstruction inst);
  void divwux(UGeckoInstruction inst);
  void divwx(UGeckoInstruction inst);
  void srawix(UGeckoInstruction inst);
  void srawx(UGeckoInstruction inst);
  void arithXex(UGeckoInstruction inst);

  void extsXx(UGeckoInstruction inst);

  void sc(UGeckoInstruction _inst);
  void rfi(UGeckoInstruction _inst);

  void bx(UGeckoInstruction inst);
  void bclrx(UGeckoInstruction _inst);
  void bcctrx(UGeckoInstruction _inst);
  void bcx(UGeckoInstruction inst);

  void mtspr(UGeckoInstruction inst);
  void mfspr(UGeckoInstruction inst);
  void mtmsr(UGeckoInstruction inst);
  void mfmsr(UGeckoInstruction inst);
  void mftb(UGeckoInstruction inst);
  void mtcrf(UGeckoInstruction inst);
  void mfcr(UGeckoInstruction inst);
  void mcrf(UGeckoInstruction inst);
  void mcrxr(UGeckoInstruction inst);
  void mcrfs(UGeckoInstruction inst);
  void mffsx(UGeckoInstruction inst);
  void mtfsb0x(UGeckoInstruction inst);
  void mtfsb1x(UGeckoInstruction inst);
  void mtfsfix(UGeckoInstruction inst);
  void mtfsfx(UGeckoInstruction inst);

  void boolX(UGeckoInstruction inst);
  void crXXX(UGeckoInstruction inst);

  void reg_imm(UGeckoInstruction inst);

  void ps_mr(UGeckoInstruction inst);
  void ps_mergeXX(UGeckoInstruction inst);
  void ps_res(UGeckoInstruction inst);
  void ps_rsqrte(UGeckoInstruction inst);
  void ps_sum(UGeckoInstruction inst);
  void ps_muls(UGeckoInstruction inst);
  void ps_cmpXX(UGeckoInstruction inst);

  void fp_arith(UGeckoInstruction inst);

  void fcmpX(UGeckoInstruction inst);
  void fctiwx(UGeckoInstruction inst);
  void fmrx(UGeckoInstruction inst);
  void frspx(UGeckoInstruction inst);
  void frsqrtex(UGeckoInstruction inst);
  void fresx(UGeckoInstruction inst);

  void cmpXX(UGeckoInstruction inst);

  void cntlzwx(UGeckoInstruction inst);

  void lfXXX(UGeckoInstruction inst);
  void stfXXX(UGeckoInstruction inst);
  void stfiwx(UGeckoInstruction inst);
  void psq_lXX(UGeckoInstruction inst);
  void psq_stXX(UGeckoInstruction inst);

  void fmaddXX(UGeckoInstruction inst);
  void fsign(UGeckoInstruction inst);
  void fselx(UGeckoInstruction inst);
  void stX(UGeckoInstruction inst);  // stw sth stb
  void rlwinmx(UGeckoInstruction inst);
  void rlwimix(UGeckoInstruction inst);
  void rlwnmx(UGeckoInstruction inst);
  void negx(UGeckoInstruction inst);
  void slwx(UGeckoInstruction inst);
  void srwx(UGeckoInstruction inst);
  void dcbt(UGeckoInstruction inst);
  void dcbz(UGeckoInstruction inst);

  void subfic(UGeckoInstruction inst);
  void subfx(UGeckoInstruction inst);

  void twX(UGeckoInstruction inst);

  void lXXx(UGeckoInstruction inst);

  void stXx(UGeckoInstruction inst);

  void lmw(UGeckoInstruction inst);
  void stmw(UGeckoInstruction inst);

  void dcbx(UGeckoInstruction inst);

  void eieio(UGeckoInstruction inst);

private:
  static void InitializeInstructionTables();
  void CompileInstruction(PPCAnalyst::CodeOp& op);

  bool HandleFunctionHooking(u32 address);

  void AllocStack();
  void FreeStack();

  JitBlockCache blocks{*this};
  TrampolineCache trampolines{*this};

  GPRRegCache gpr{*this};
  FPURegCache fpr{*this};

  Jit64AsmRoutineManager asm_routines{*this};

  bool m_enable_blr_optimization;
  bool m_cleanup_after_stackfault;
  u8* m_stack;
};

void LogGeneratedX86(size_t size, const PPCAnalyst::CodeBuffer& code_buffer, const u8* normalEntry,
                     const JitBlock* b);
