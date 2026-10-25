// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>

#include "Common/PPC64Emitter.h"
#include "Common/RangeSizeSet.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitPPC64/JitPPC64Cache.h"
#include "Core/PowerPC/JitPPC64/JitPPC64_RegCache.h"
#include "Core/PowerPC/PPCAnalyst.h"

class HostDisassembler;

// The PowerPCState pointer. Non-volatile, so that it survives calls.
constexpr PPC64Gen::GPR PPC_REG = PPC64Gen::GPR::R31;
// The guest PC when entering the dispatcher. Non-volatile, so that it survives calls.
constexpr PPC64Gen::GPR DISPATCHER_PC = PPC64Gen::GPR::R30;
// The base of the fastmem arena for the current MSR[DR], PowerPCState::mem_ptr. Non-volatile, so
// that it survives calls.
constexpr PPC64Gen::GPR MEM_REG = PPC64Gen::GPR::R29;
// PowerPCState::downcount, sign-extended, while JIT code runs. C++ code reads and changes it in
// PowerPCState, so it is stored there before calls and reloaded after. Non-volatile.
constexpr PPC64Gen::GPR DOWNCOUNT_REG = PPC64Gen::GPR::R28;

// Offsets into PowerPCState, for D-form loads and stores relative to PPC_REG.
#ifdef __GNUC__
#define PPCSTATE_OFF(elem)                                                                         \
  ([]() consteval {                                                                                \
    _Pragma("GCC diagnostic push") _Pragma(                                                        \
        "GCC diagnostic ignored \"-Winvalid-offsetof\"") constexpr size_t ppcstate_offset =        \
        offsetof(PowerPC::PowerPCState, elem);                                                     \
    _Pragma("GCC diagnostic pop") static_assert(ppcstate_offset < 0x8000);                         \
    return static_cast<s16>(ppcstate_offset);                                                      \
  }())
#else
#define PPCSTATE_OFF(elem) (static_cast<s16>(offsetof(PowerPC::PowerPCState, elem)))
#endif

#define PPCSTATE_OFF_ARRAY(elem, i)                                                                \
  (static_cast<s16>(PPCSTATE_OFF(elem) + sizeof(PowerPC::PowerPCState::elem[0]) * (i)))
#define PPCSTATE_OFF_GPR(i) PPCSTATE_OFF_ARRAY(gpr, i)
#define PPCSTATE_OFF_CR(i) PPCSTATE_OFF_ARRAY(cr.fields, i)
#define PPCSTATE_OFF_SPR(i) PPCSTATE_OFF_ARRAY(spr, i)
#define PPCSTATE_OFF_PS(i) PPCSTATE_OFF_ARRAY(ps, i)

// Calls the interpreter for this instruction, with only the registers it uses written back: see
// JitPPC64::InterpretInstruction.
#define INTERPRET_IF(cond)                                                                         \
  do                                                                                               \
  {                                                                                                \
    if (cond)                                                                                      \
    {                                                                                              \
      InterpretInstruction(inst);                                                                  \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

class JitPPC64 : public JitBase, public PPC64Gen::PPC64CodeBlock, public CommonAsmRoutinesBase
{
public:
  explicit JitPPC64(Core::System& system);
  JitPPC64(const JitPPC64&) = delete;
  JitPPC64(JitPPC64&&) = delete;
  JitPPC64& operator=(const JitPPC64&) = delete;
  JitPPC64& operator=(JitPPC64&&) = delete;
  ~JitPPC64() override;

  [[nodiscard]] bool Init() override;
  void Shutdown() override;

  JitBaseBlockCache* GetBlockCache() override { return &blocks; }

  bool HandleFault(uintptr_t access_address, SContext* ctx) override;

  void ClearCache() override;
  // The copy of a routine in the chunk of the code at code, or of the code being written.
  const u8* Routine(const u8* routine, const u8* code) const;
  const u8* Routine(const u8* routine) const { return Routine(routine, GetCodePtr()); }

  CommonAsmRoutinesBase* GetAsmRoutines() override { return this; }

  void Run() override;
  void SingleStep() override;

  void Jit(u32 em_address) override;
  void Jit(u32 em_address, bool clear_cache_and_retry_on_failure);

  void EraseSingleBlock(const JitBlock& block) override;
  std::vector<MemoryStats> GetMemoryStats() const override;

  std::size_t DisassembleNearCode(const JitBlock& block, std::ostream& stream) const override;
  std::size_t DisassembleFarCode(const JitBlock& block, std::ostream& stream) const override;

  const char* GetName() const override { return "JITPPC64"; }

  // OPCODES
  using Instruction = void (JitPPC64::*)(UGeckoInstruction);
  void FallBackToInterpreter(UGeckoInstruction inst);
  void InterpretInstruction(UGeckoInstruction inst);
  void DoNothing(UGeckoInstruction inst);

  void DynaRunTable4(UGeckoInstruction inst);
  void DynaRunTable19(UGeckoInstruction inst);
  void DynaRunTable31(UGeckoInstruction inst);
  void DynaRunTable59(UGeckoInstruction inst);
  void DynaRunTable63(UGeckoInstruction inst);

  // Branch
  void sc(UGeckoInstruction inst);
  void bx(UGeckoInstruction inst);
  void bcx(UGeckoInstruction inst);
  void bcctrx(UGeckoInstruction inst);
  void bclrx(UGeckoInstruction inst);
  void rfi(UGeckoInstruction inst);

  // Integer
  void arith_imm(UGeckoInstruction inst);
  void boolX(UGeckoInstruction inst);
  void addx(UGeckoInstruction inst);
  void addix(UGeckoInstruction inst);
  void addic(UGeckoInstruction inst);
  void addcx(UGeckoInstruction inst);
  void addex(UGeckoInstruction inst);
  void addzex(UGeckoInstruction inst);
  void subfx(UGeckoInstruction inst);
  void subfex(UGeckoInstruction inst);
  void subfzex(UGeckoInstruction inst);
  void subfic(UGeckoInstruction inst);
  void negx(UGeckoInstruction inst);
  void twx(UGeckoInstruction inst);
  void mulli(UGeckoInstruction inst);
  void mullwx(UGeckoInstruction inst);
  void mulhwx(UGeckoInstruction inst);
  void mulhwux(UGeckoInstruction inst);
  void divwx(UGeckoInstruction inst);
  void divwux(UGeckoInstruction inst);
  void extsXx(UGeckoInstruction inst);
  void cntlzwx(UGeckoInstruction inst);
  void slwx(UGeckoInstruction inst);
  void srwx(UGeckoInstruction inst);
  void srawx(UGeckoInstruction inst);
  void srawix(UGeckoInstruction inst);
  void rlwinmx(UGeckoInstruction inst);
  void rlwnmx(UGeckoInstruction inst);
  void rlwimix(UGeckoInstruction inst);
  void cmp(UGeckoInstruction inst);
  void cmpl(UGeckoInstruction inst);
  void cmpi(UGeckoInstruction inst);
  void cmpli(UGeckoInstruction inst);

  // Loads and stores
  void lXX(UGeckoInstruction inst);
  void stX(UGeckoInstruction inst);
  void dcbx(UGeckoInstruction inst);
  void dcbz(UGeckoInstruction inst);
  void lmw_stmw(UGeckoInstruction inst);

  // Floating-point loads and stores
  void lfXX(UGeckoInstruction inst);
  void stfXX(UGeckoInstruction inst);

  // Floating point
  void fp_arith(UGeckoInstruction inst);
  void fp_logic(UGeckoInstruction inst);
  void fselx(UGeckoInstruction inst);
  void frspx(UGeckoInstruction inst);
  void fresx(UGeckoInstruction inst);
  void frsqrtex(UGeckoInstruction inst);
  void fctiwx(UGeckoInstruction inst);
  void fcmpX(UGeckoInstruction inst);

  // Quantized loads and stores
  void psq_lXX(UGeckoInstruction inst);
  // The double 2^exponent in doubleword 0, or with splat in both.
  void LoadPowerOfTwo(PPC64Gen::VSR out, PPC64Gen::GPR tmp, s32 exponent, bool splat = false);
  void psq_stXX(UGeckoInstruction inst);

  // Paired singles
  void ps_arith(UGeckoInstruction inst);
  void ps_sumX(UGeckoInstruction inst);
  void ps_res(UGeckoInstruction inst);
  void ps_rsqrte(UGeckoInstruction inst);
  void ps_sel(UGeckoInstruction inst);
  void ps_cmpXX(UGeckoInstruction inst);
  void ps_mergeXX(UGeckoInstruction inst);

  // System registers
  void mfspr(UGeckoInstruction inst);
  void mtspr(UGeckoInstruction inst);
  void mfcr(UGeckoInstruction inst);
  void mtcrf(UGeckoInstruction inst);
  void mcrf(UGeckoInstruction inst);
  void mfmsr(UGeckoInstruction inst);
  void EmitTimeBase(PPC64Gen::GPR out);
  void EmitMSRUpdated(PPC64Gen::GPR msr);
  void EmitPendingPageTableUpdate();
  void mtmsr(UGeckoInstruction inst);
  void crXXX(UGeckoInstruction inst);
  void mcrxr(UGeckoInstruction inst);
  void mfsr(UGeckoInstruction inst);
  void mcrfs(UGeckoInstruction inst);
  void mffsx(UGeckoInstruction inst);
  void mtfsb0x(UGeckoInstruction inst);
  void mtfsb1x(UGeckoInstruction inst);
  void mtfsfix(UGeckoInstruction inst);
  void mtfsfx(UGeckoInstruction inst);
  void EmitFPSCRUpdated(PPC64Gen::GPR fpscr, bool rounding_mode_changed);

  // The stack frame saves the scratch registers, which are volatile, around calls from slow paths.
  static constexpr s32 SCRATCH_SAVE = PPC64Gen::STACK_FRAME_MIN_SIZE + 8 * 8;
  static constexpr s32 SCRATCH_SAVE_SIZE = 8 * 7;
  // And LR, in routines that make calls.
  static constexpr s32 ROUTINE_LR_SAVE = SCRATCH_SAVE + SCRATCH_SAVE_SIZE;
  // And LR and another value in the quantized load and store routines, which call those.
  static constexpr s32 QUANTIZED_LR_SAVE = ROUTINE_LR_SAVE + 8;
  static constexpr s32 QUANTIZED_VALUE_SAVE = QUANTIZED_LR_SAVE + 8;
  // With the BLR optimization, a guest call pushes a frame with the host and guest return addresses
  // (the guest's with the feature flags in its upper 32 bits), which the guest's return pops. JIT
  // code and its calls use the top frame as they use the dispatcher's, so the addresses go after
  // what they use.
  static constexpr s32 BLR_HOST_RETURN = QUANTIZED_VALUE_SAVE + 8;
  static constexpr s32 BLR_GUEST_RETURN = BLR_HOST_RETURN + 8;
  static constexpr s32 BLR_FRAME_SIZE = BLR_GUEST_RETURN + 8;
  static_assert(BLR_FRAME_SIZE % 16 == 0, "The stack pointer must stay 16-byte aligned");
  static constexpr s32 ScratchSaveOffset(PPC64Gen::GPR reg)
  {
    return SCRATCH_SAVE + 8 * (PPC64Gen::DecodeReg(reg) - 5);
  }

protected:
  // Calls into C++ code, with the downcount in PowerPCState for it.
  template <typename FuncRet, typename... FuncArgs, typename... Args>
  void ABI_CallFunction(FuncRet (*func)(FuncArgs...), Args... args)
  {
    STW(DOWNCOUNT_REG, PPC_REG, PPCSTATE_OFF(downcount));
    PPC64CodeBlock::ABI_CallFunction(func, args...);
    LWA(DOWNCOUNT_REG, PPC_REG, PPCSTATE_OFF(downcount));
  }

  struct MemAccess
  {
    bool store = false;
    u32 size = 0;
    bool sign_extend = false;
    // lwbrx, lhbrx, stwbrx and sthbrx
    bool reverse = false;
  };

  // Puts the effective address of a load or store, zero-extended, in addr.
  void ComputeAddress(PPC64Gen::GPR addr, UGeckoInstruction inst, bool indexed);
  // Loads into or stores from value, at the address in addr.
  void EmitMemoryAccess(MemAccess access, PPC64Gen::GPR value, PPC64Gen::GPR addr);
  bool IsGatherPipeStore() const;
  // Loads into doubleword 0 of raw what EmitMemoryAccess would load into value, for 64 bits, or
  // into word 0 for 32 bits; stores doubleword 0 of src, or its low word. With Power ISA 3.0 and
  // fastmem, without moving between GPRs and VSRs, unless the access takes the slow path.
  void EmitLoadToVSR(u32 size, PPC64Gen::VSR raw, PPC64Gen::GPR value, PPC64Gen::GPR addr);
  void EmitStoreFromVSR(u32 size, PPC64Gen::VSR src, PPC64Gen::GPR value, PPC64Gen::GPR addr);
  void EmitMemoryAccessToAddress(MemAccess access, PPC64Gen::GPR value, PPC64Gen::GPR addr);
  void EmitFastAccess(MemAccess access, PPC64Gen::GPR value, PPC64Gen::GPR addr,
                      PPC64Gen::GPR base = MEM_REG);
  // Saves the scratch GPRs in use, but not those in except, around a call. Returns the ones saved.
  BitSet32 SaveScratchRegs(BitSet32 except = {});
  void RestoreScratchRegs(BitSet32 saved);
  void EmitSlowAccess(MemAccess access, PPC64Gen::GPR value, PPC64Gen::GPR addr);
  static u32 SlowAccessRoutineIndex(MemAccess access);
  void GenerateSlowAccessRoutines();
  void GenerateExceptionExitRoutine();
  void GenerateQuantizedRoutines();
  void GenerateFPRFRoutine(bool single);
  void GenerateFPRFRoutineISA3_0(bool single);
  void GenerateReciprocalEstimateRoutine();
  void GenerateReciprocalSquareRootEstimateRoutine();
  void QuantizedLoadStoreWithRoutine(UGeckoInstruction inst, bool store, bool indexed, bool update,
                                     u32 gqr_index, bool single);
  void GenerateQuantizedLoadRoutine(u32 type, bool single);
  void GenerateQuantizedStoreRoutine(u32 type, bool single);
  std::array<PPC64Gen::FixupBranch, 3> EmitQuantizedRAMAddress(u32 size);
  void GenerateSlowAccessRoutine(MemAccess access, bool translate_inline);
  void EmitTranslatedRAMAccess(MemAccess access);
  bool HandleFastmemFault(SContext* ctx);

  // Slow paths go in far code, out of the way of the code that runs.
  void SwitchToFarCode();
  void SwitchToNearCode();
  // Conditional branches only reach +/- 32 KiB, so a branch to far code skips over an
  // unconditional one if the condition is false. Returns the unconditional branch.
  [[nodiscard]] PPC64Gen::FixupBranch BCFar(PPC64Gen::CCFlags cond, PPC64Gen::CRF field);

  void FlushRegisters(FlushMode mode);

  // Floating point.
  //
  // FPSCR[NI] makes the guest flush denormal results to zero, which the host can't do (see
  // Common::FPU::SetSIMDMode), so code with NI set takes a slow path in far code. CR4 holds NI,
  // as EQ if it's clear: CR4 is non-volatile, so it's loaded once per block, and again after
  // calls into code that can change the FPSCR.
  void EnsureNILoaded();
  // Flushes a denormal double-precision result in reg to zero if NI is set, as ForceDouble.
  void ForceDouble(PPC64Gen::VSR reg);
  // Rounds a double-precision result in reg to single precision, as ForceSingle.
  void ForceSingle(PPC64Gen::VSR reg);
  // ForceSingle for both halves.
  void ForcePairedSingle(PPC64Gen::VSR reg);
  // From far code, calls the interpreter for a floating-point instruction, with its operands and
  // FD in PowerPCState and FD reloaded after, then branches to done.
  void InterpretFromFarCode(UGeckoInstruction inst, const u8* done);
  // Rounds the mantissa of a multiplicand to 25 bits, as Force25Bit.
  void Force25Bit(PPC64Gen::VSR out, PPC64Gen::VSR in);
  // Whether the FPR holds singles, which a multiplicand needn't be rounded from.
  bool IsSingle(u32 reg) const;
  // Sets a CR field from the host CR field that a floating-point comparison set.
  void SetFPRF(PPC64Gen::VSR value, bool single);
  void EmitEstimate(UGeckoInstruction inst, bool rsqrte, bool paired);
  void SetFPCCFromHostFloatCompare(PPC64Gen::CRF host_field);
  void SetCRFieldFromHostFloatCompare(u32 field, PPC64Gen::CRF host_field);
  // Converts the double in the low 64 bits of value to single precision, as ConvertToSingle.
  void ConvertDoubleToSingle(PPC64Gen::GPR value);
  // Converts the double in the low 64 bits of value to single precision, as ConvertToSingleFTZ.
  void ConvertDoubleToSingleFTZ(PPC64Gen::GPR value);
  void ConvertDoubleToSingleFTZ(PPC64Gen::GPR value, PPC64Gen::GPR exp, PPC64Gen::GPR high);
  // Quantizes doubleword 0 of in into out for psq_st, as ScaleAndClamp.
  void QuantizeHalf(PPC64Gen::GPR out, PPC64Gen::VSR in, u32 type_size, bool is_signed,
                    s32 exponent);
  void QuantizeHalf(PPC64Gen::GPR out, PPC64Gen::VSR in, u32 type_size, bool is_signed,
                    PPC64Gen::VSR factor, PPC64Gen::VSR scaled, PPC64Gen::GPR bound);
  // With Power ISA 3.0, both halves of psq_l and psq_st of integers at once, scaled by factor in
  // both doublewords, or not scaled for VS0. DequantizePair converts the integers in hi and lo,
  // extended in place if signed, into out. QuantizePair quantizes both halves of in into out, half
  // 0 in the upper bits, as QuantizeHalf does each; it uses tmp, scaled and work.
  void DequantizePair(PPC64Gen::VSR out, PPC64Gen::GPR hi, PPC64Gen::GPR lo, u32 type_size,
                      bool is_signed, PPC64Gen::VSR factor);
  void QuantizePair(PPC64Gen::GPR out, PPC64Gen::GPR tmp, PPC64Gen::VSR in, u32 type_size,
                    bool is_signed, PPC64Gen::VSR factor, PPC64Gen::VSR scaled, PPC64Gen::VSR work);
  // Computes the result of fp_arith's operation op5 (SUBOP5) into result, from doubleword 0 of a,
  // b and c, rounded to single precision if single, as the interpreter does.
  void EmitFloatArith(u32 op5, bool single, bool round_c, PPC64Gen::VSR result, PPC64Gen::VSR a,
                      PPC64Gen::VSR b, PPC64Gen::VSR c);
  // Paired-single arithmetic on both halves at once, as EmitFloatArith on each. c_half is the half
  // of C for both halves of ps_muls0 and ps_madds0 (0) or ps_muls1 and ps_madds1 (1), or -1.
  void EmitPairedArith(UGeckoInstruction inst, u32 op5, int c_half, PPC64Gen::VSR d,
                       PPC64Gen::VSR a, PPC64Gen::VSR b, PPC64Gen::VSR c);

  void HLEFunction(u32 hook_index);
  bool HandleFunctionHooking(u32 address);
  void SetOptimizationEnabled(bool enabled);
  void CompileInstruction(PPCAnalyst::CodeOp& op);

  bool DoJit(u32 em_address, JitBlock* b, u32 nextPC);

  bool SetEmitterStateToFreeCodeRegion();
  void FreeRanges();
  void ResetFreeMemoryRanges(size_t routines_size);
  void GenerateAsmAndResetFreeMemoryRanges();
  size_t CodeChunk(const u8* code) const;
  void GenerateAsm();

  // Exits from a block. The guest registers must be flushed first. All of them subtract the block's
  // cycles from the downcount, which leaves CR0 set for the dispatcher.
  void Cleanup();
  void DoDownCount();
  void EndProfiling();
  // Exits to a known address, which can be linked to the block there. With LK, a guest call, which
  // returns to exit_address_after_return.
  void WriteExit(u32 destination, bool LK = false, u32 exit_address_after_return = 0);
  // Exits to the address in a register, through the dispatcher.
  void WriteExit(PPC64Gen::GPR dest, bool LK = false, u32 exit_address_after_return = 0);
  // A guest return, to the address in a register: to the host code after the guest call, if the
  // address is the one it returns to, or else through the dispatcher.
  void WriteBLRExit(PPC64Gen::GPR dest);
  // A guest call that the block follows, whose return is in another block.
  void FakeLKExit(u32 exit_address_after_return);
  // Pushes a frame for a guest call returning to exit_address_after_return, and to the host code
  // host_offset bytes after what this emits. Uses R4.
  void PushReturnFrame(u32 exit_address_after_return, s32 host_offset);
  // The exit to the guest return address, where the guest's return comes back to.
  void WriteReturnExit(u32 exit_address_after_return);
  // Drops the frames that guest calls pushed.
  void ResetStack();
  // Checks for exceptions, then exits through the dispatcher.
  void WriteExceptionExit(u32 destination, bool only_external = false,
                          bool always_exception = false);
  void WriteExceptionExit(PPC64Gen::GPR dest, bool only_external = false,
                          bool always_exception = false);
  // Exits at the current instruction if the given exception is pending.
  // Takes the pending exception at this instruction.
  void WriteTakeExceptionExit();
  void WriteProgramExceptionExit(u32 cause);
  void WriteConditionalExceptionExit(u32 exception, PPC64Gen::GPR tmp = PPC64Gen::GPR::R3);

  void LogGeneratedCode() const;

  // Sets CR0 from the 32-bit result in value, with SO copied from XER, as the interpreter does.
  void ComputeRC0(PPC64Gen::GPR value);
  // Sets a CR field from comparing a with b, or with imm, signed or unsigned.
  void SetCRFieldFromCompare(u32 field, PPC64Gen::GPR a, PPC64Gen::GPR b, bool is_signed);
  void SetCRFieldFromCompare(u32 field, PPC64Gen::GPR a, s32 imm, bool is_signed);
  // Puts XER[SO] in the CR value in cr, a sign-extended value or difference.
  void SetCRFieldSO(PPC64Gen::GPR cr);
  // Converts between a CR field and its four PowerPC bits (LT, GT, EQ, SO), in the low bits.
  void GetCRField(PPC64Gen::GPR out, u32 field);
  void SetCRField(u32 field, PPC64Gen::GPR value);
  // Branches if the CR bit (field, bit) is set (or clear). Uses CR0, or CR7 after a compare for
  // the branch.
  [[nodiscard]] PPC64Gen::FixupBranch JumpIfCRFieldBit(u32 field, u32 bit, bool jump_if_set);
  // If the next instruction branches on LT, GT or EQ of the CR field, compares a with b, or with
  // imm, into host CR7 too, so that the branch doesn't wait for the field's value, as Jit64 merges
  // them.
  bool NextBranchTestsCRField(u32 field) const;
  void CompareForNextBranch(u32 field, PPC64Gen::GPR a, PPC64Gen::GPR b, bool is_signed);
  void CompareForNextBranch(u32 field, PPC64Gen::GPR a, s32 imm, bool is_signed);

  // d = x + y + carry_in, in 32 bits, setting XER[CA] to the carry out.
  enum class CarryIn
  {
    Zero,
    One,
    XER,
  };
  void AddWithCarry(PPC64Gen::GPR d, PPC64Gen::GPR x, PPC64Gen::GPR y, CarryIn carry_in);
  // Sets XER[CA] from the host's, after an algebraic shift.
  void StoreHostCarry();
  void ComputeAddOverflow(PPC64Gen::GPR overflow, PPC64Gen::GPR x, PPC64Gen::GPR y,
                          CarryIn carry_in);
  void ComputeOverflowOf64(PPC64Gen::GPR overflow, PPC64Gen::GPR value);
  void StoreOverflow(PPC64Gen::GPR overflow);

  PPC64GPRCache gpr;
  PPC64FPRCache fpr;
  bool m_ni_loaded = false;
  // The CR field that the previous instruction compared into host CR7 too, and the one that the
  // instruction being compiled does.
  std::optional<u32> m_cr7_field;
  std::optional<u32> m_next_cr7_field;

  // The Power ISA levels whose instructions the JIT uses: the host's, or lower for testing, from
  // DOLPHIN_PPC64_ISA (2.07 or 3.0).
  bool m_isa_3_0 = false;
  bool m_isa_3_1 = false;
  // For testing, DOLPHIN_PPC64_CODE_CHUNK_LIMIT limits the near and the far code of each chunk to
  // that many bytes, so that blocks are spread over the chunks, link between them, and fill the
  // code space often.
  size_t m_code_chunk_limit = 0;

  // With Power ISA 3.1, constants that pli can't load are loaded with pld from this pool, which
  // is in the code space, within its reach. The routines are copied to each chunk, so they don't
  // use it.
  bool LoadConstantFromPool(PPC64Gen::GPR rd, u64 imm) override;
  static constexpr size_t CONSTANT_POOL_SIZE = 64 * 1024;
  u64* m_constant_pool = nullptr;
  size_t m_constant_pool_count = 0;
  std::unordered_map<u64, const u64*> m_constant_pool_entries;
  bool m_generating_routines = false;

  JitPPC64BlockCache blocks{*this};

  // Unconditional branches reach +/- 32 MiB. The code space is made of chunks of that size, which
  // each start with a copy of the routines, followed by near code and far code. A block's code is
  // all in one chunk, so that it can branch to the routines, and between near and far code;
  // links to blocks in other chunks go through the dispatcher when they're out of range.
  static constexpr size_t CODE_CHUNK_SIZE = 32 * 1024 * 1024;
  static constexpr size_t CODE_CHUNKS = 4;
  // Each memory access has a slow path in far code, so far code is about 80% the size of near code.
  static constexpr size_t NEAR_CODE_SIZE = 18 * 1024 * 1024;

  std::array<Common::RangeSizeSet<u8*>, CODE_CHUNKS> m_free_ranges_near;
  std::array<Common::RangeSizeSet<u8*>, CODE_CHUNKS> m_free_ranges_far;

  // The emitter state of the code that isn't being written to.
  u8* m_other_code = nullptr;
  u8* m_other_code_end = nullptr;
  bool m_other_code_write_failed = false;
  bool m_in_far_code = false;

  // The fastmem accesses that haven't faulted yet, and their slow paths.
  std::map<const u8*, const u8*> m_fault_to_handler;

  std::array<const u8*, 32> m_slow_access_routines{};
  // Takes a pending exception at the PC in R3, after subtracting the cycles in R4.
  const u8* m_exception_exit_routine = nullptr;
  // Quantized loads and stores whose GQR isn't known when compiling, indexed by W << 3 | type.
  std::array<const u8*, 16> m_quantized_load_routines{};
  std::array<const u8*, 16> m_quantized_store_routines{};
  // Set FPSCR[FPRF] for the value in R3, double or rounded to single precision.
  const u8* m_fprf_double_routine = nullptr;
  const u8* m_fprf_single_routine = nullptr;
  // Common::ApproximateReciprocal and ApproximateReciprocalSquareRoot of the double in R3, into
  // R3, with R4 zero; or R4 nonzero for inputs whose instructions the interpreter has to run.
  const u8* m_fres_routine = nullptr;
  const u8* m_frsqrte_routine = nullptr;

  std::unique_ptr<HostDisassembler> m_disassembler;
};
