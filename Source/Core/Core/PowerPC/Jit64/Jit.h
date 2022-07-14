// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include <rangeset/rangesizeset.h>

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
  void Jit(u32 em_address, bool clear_cache_and_retry_on_failure);
  bool DoJit(u32 em_address, JitBlock* b, u32 nextPC);

  // Finds a free memory region and sets the near and far code emitters to point at that region.
  // Returns false if no free memory region can be found for either of the two.
  bool SetEmitterStateToFreeCodeRegion();

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
  void GenerateOverflow(Gen::CCFlags cond = Gen::CCFlags::CC_NO);
  void FinalizeCarryOverflow(bool oe, bool inv = false);
  void FinalizeCarry(Gen::CCFlags cond);
  void FinalizeCarry(bool ca);
  void ComputeRC(preg_t preg, bool needs_test = true, bool needs_sext = true);

  void AndWithMask(Gen::X64Reg reg, u32 mask);
  void RotateLeft(int bits, Gen::X64Reg regOp, const Gen::OpArg& arg, u8 rotate);

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
  void FixGTBeforeSettingCRFieldBit(Gen::X64Reg reg);
  // Generates a branch that will check if a given bit of a CR register part
  // is set or not.
  Gen::FixupBranch JumpIfCRFieldBit(int field, int bit, bool jump_if_set = true);

  void UpdateFPExceptionSummary(Gen::X64Reg fpscr, Gen::X64Reg tmp1, Gen::X64Reg tmp2);

  void SetFPRFIfNeeded(const Gen::OpArg& xmm, bool single);
  void FinalizeSingleResult(Gen::X64Reg output, const Gen::OpArg& input, bool packed = true,
                            bool duplicate = false);
  void FinalizeDoubleResult(Gen::X64Reg output, const Gen::OpArg& input);
  void HandleNaNs(GeckoInstruction inst, Gen::X64Reg xmm, Gen::X64Reg clobber);

  void MultiplyImmediate(u32 imm, int a, int d, bool overflow);

  typedef u32 (*Operation)(u32 a, u32 b);
  void regimmop(int d, int a, bool binary, u32 value, Operation doop,
                void (Gen::XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&),
                bool Rc = false, bool carry = false);
  void FloatCompare(GeckoInstruction inst, bool upper = false);
  void UpdateMXCSR();

  // OPCODES
  using Instruction = void (Jit64::*)(GeckoInstruction instCode);
  void FallBackToInterpreter(GeckoInstruction _inst);
  void DoNothing(GeckoInstruction _inst);
  void HLEFunction(u32 hook_index);

  void DynaRunTable4(GeckoInstruction inst);
  void DynaRunTable19(GeckoInstruction inst);
  void DynaRunTable31(GeckoInstruction inst);
  void DynaRunTable59(GeckoInstruction inst);
  void DynaRunTable63(GeckoInstruction inst);

  void addx(GeckoInstruction inst);
  void mulli(GeckoInstruction inst);
  void mulhwXx(GeckoInstruction inst);
  void mullwx(GeckoInstruction inst);
  void divwux(GeckoInstruction inst);
  void divwx(GeckoInstruction inst);
  void srawix(GeckoInstruction inst);
  void srawx(GeckoInstruction inst);
  void arithXex(GeckoInstruction inst);

  void extsXx(GeckoInstruction inst);

  void sc(GeckoInstruction _inst);
  void rfi(GeckoInstruction _inst);

  void bx(GeckoInstruction inst);
  void bclrx(GeckoInstruction _inst);
  void bcctrx(GeckoInstruction _inst);
  void bcx(GeckoInstruction inst);

  void mtspr(GeckoInstruction inst);
  void mfspr(GeckoInstruction inst);
  void mtmsr(GeckoInstruction inst);
  void mfmsr(GeckoInstruction inst);
  void mftb(GeckoInstruction inst);
  void mtcrf(GeckoInstruction inst);
  void mfcr(GeckoInstruction inst);
  void mcrf(GeckoInstruction inst);
  void mcrxr(GeckoInstruction inst);
  void mcrfs(GeckoInstruction inst);
  void mffsx(GeckoInstruction inst);
  void mtfsb0x(GeckoInstruction inst);
  void mtfsb1x(GeckoInstruction inst);
  void mtfsfix(GeckoInstruction inst);
  void mtfsfx(GeckoInstruction inst);

  void boolX(GeckoInstruction inst);
  void crXXX(GeckoInstruction inst);

  void reg_imm(GeckoInstruction inst);

  void ps_mr(GeckoInstruction inst);
  void ps_mergeXX(GeckoInstruction inst);
  void ps_res(GeckoInstruction inst);
  void ps_rsqrte(GeckoInstruction inst);
  void ps_sum(GeckoInstruction inst);
  void ps_muls(GeckoInstruction inst);
  void ps_cmpXX(GeckoInstruction inst);

  void fp_arith(GeckoInstruction inst);

  void fcmpX(GeckoInstruction inst);
  void fctiwx(GeckoInstruction inst);
  void fmrx(GeckoInstruction inst);
  void frspx(GeckoInstruction inst);
  void frsqrtex(GeckoInstruction inst);
  void fresx(GeckoInstruction inst);

  void cmpXX(GeckoInstruction inst);

  void cntlzwx(GeckoInstruction inst);

  void lfXXX(GeckoInstruction inst);
  void stfXXX(GeckoInstruction inst);
  void stfiwx(GeckoInstruction inst);
  void psq_lXX(GeckoInstruction inst);
  void psq_stXX(GeckoInstruction inst);

  void fmaddXX(GeckoInstruction inst);
  void fsign(GeckoInstruction inst);
  void fselx(GeckoInstruction inst);
  void stX(GeckoInstruction inst);  // stw sth stb
  void rlwinmx(GeckoInstruction inst);
  void rlwimix(GeckoInstruction inst);
  void rlwnmx(GeckoInstruction inst);
  void negx(GeckoInstruction inst);
  void slwx(GeckoInstruction inst);
  void srwx(GeckoInstruction inst);
  void dcbt(GeckoInstruction inst);
  void dcbz(GeckoInstruction inst);

  void subfic(GeckoInstruction inst);
  void subfx(GeckoInstruction inst);

  void twX(GeckoInstruction inst);

  void lXXx(GeckoInstruction inst);

  void stXx(GeckoInstruction inst);

  void lmw(GeckoInstruction inst);
  void stmw(GeckoInstruction inst);

  void dcbx(GeckoInstruction inst);

  void eieio(GeckoInstruction inst);

private:
  void CompileInstruction(PPCAnalyst::CodeOp& op);

  bool HandleFunctionHooking(u32 address);

  void AllocStack();
  void FreeStack();

  void ResetFreeMemoryRanges();

  JitBlockCache blocks{*this};
  TrampolineCache trampolines{*this};

  GPRRegCache gpr{*this};
  FPURegCache fpr{*this};

  Jit64AsmRoutineManager asm_routines{*this};

  bool m_enable_blr_optimization = false;
  bool m_cleanup_after_stackfault = false;
  u8* m_stack = nullptr;

  HyoutaUtilities::RangeSizeSet<u8*> m_free_ranges_near;
  HyoutaUtilities::RangeSizeSet<u8*> m_free_ranges_far;
};

void LogGeneratedX86(size_t size, const PPCAnalyst::CodeBuffer& code_buffer, const u8* normalEntry,
                     const JitBlock* b);
