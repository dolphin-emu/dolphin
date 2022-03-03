// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/x64Emitter.h"

namespace DSP::JIT::x64
{
class DSPEmitter;

enum DSPJitRegSpecial
{
  DSP_REG_AX0_32 = 32,
  DSP_REG_AX1_32 = 33,
  DSP_REG_ACC0_64 = 34,
  DSP_REG_ACC1_64 = 35,
  DSP_REG_PROD_64 = 36,
  DSP_REG_MAX_MEM_BACKED = 36,

  DSP_REG_USED = 253,
  DSP_REG_STATIC = 254,
  DSP_REG_NONE = 255
};

enum class RegisterExtension
{
  Sign,
  Zero,
  None
};

class DSPJitRegCache
{
public:
  explicit DSPJitRegCache(DSPEmitter& emitter);

  // For branching into multiple control flows
  DSPJitRegCache(const DSPJitRegCache& cache);
  DSPJitRegCache& operator=(const DSPJitRegCache& cache);

  ~DSPJitRegCache();

  // Merge must be done _before_ leaving the code branch, so we can fix
  // up any differences in state
  void FlushRegs(DSPJitRegCache& cache, bool emit = true);
  /* since some use cases are non-trivial, some examples:

     //this does not modify the final state of gpr
     <code using gpr>
     FixupBranch b = JCC();
       DSPJitRegCache c = gpr;
       <code using c>
       gpr.FlushRegs(c);
     SetBranchTarget(b);
     <code using gpr>

     //this does not modify the final state of gpr
     <code using gpr>
     DSPJitRegCache c = gpr;
     FixupBranch b1 = JCC();
       <code using gpr>
       gpr.FlushRegs(c);
       FixupBranch b2 = JMP();
     SetBranchTarget(b1);
       <code using gpr>
       gpr.FlushRegs(c);
     SetBranchTarget(b2);
     <code using gpr>

     //this allows gpr to be modified in the second branch
     //and fixes gpr according to the results form in the first branch
     <code using gpr>
     DSPJitRegCache c = gpr;
     FixupBranch b1 = JCC();
       <code using c>
       FixupBranch b2 = JMP();
     SetBranchTarget(b1);
       <code using gpr>
       gpr.FlushRegs(c);
     SetBranchTarget(b2);
     <code using gpr>

     //this does not modify the final state of gpr
     <code using gpr>
     u8* b = GetCodePtr();
       DSPJitRegCache c = gpr;
       <code using gpr>
       gpr.FlushRegs(c);
       JCC(b);
     <code using gpr>

     this all is not needed when gpr would not be used at all in the
     conditional branch
   */

  // Drop this copy without warning
  void Drop();

  // Prepare state so that another flushed DSPJitRegCache can take over
  void FlushRegs();

  void LoadRegs(bool emit = true);  // Load statically allocated regs from memory
  void SaveRegs();                  // Save statically allocated regs to memory

  void PushRegs();  // Save registers before ABI call
  void PopRegs();   // Restore registers after ABI call

  // Returns a register with the same contents as reg that is safe
  // to use through saveStaticRegs and for ABI-calls
  Gen::X64Reg MakeABICallSafe(Gen::X64Reg reg);

  // Gives no SCALE_RIP with abs(offset) >= 0x80000000
  // 32/64 bit writes allowed when the register has a _64 or _32 suffix
  // only 16 bit writes allowed without any suffix.
  Gen::OpArg GetReg(int reg, bool load = true);
  // Done with all usages of OpArg above
  void PutReg(int reg, bool dirty = true);

  void ReadReg(int sreg, Gen::X64Reg host_dreg, RegisterExtension extend);
  void WriteReg(int dreg, Gen::OpArg arg);

  // Find a free host reg, spill if used, reserve
  Gen::X64Reg GetFreeXReg();
  // Spill a specific host reg if used, reserve
  void GetXReg(Gen::X64Reg reg);
  // Unreserve the given host reg
  void PutXReg(Gen::X64Reg reg);

private:
  struct X64CachedReg
  {
    size_t guest_reg;  // Including DSPJitRegSpecial
    bool pushed;
  };

  struct DynamicReg
  {
    Gen::OpArg loc;
    Gen::OpArg mem;
    size_t size;
    bool dirty;
    bool used;
    int last_use_ctr;
    int parentReg;
    int shift;  // Current shift if parentReg == DSP_REG_NONE
                // otherwise the shift this part can be found at
    Gen::X64Reg host_reg;

    // TODO:
    // + drop sameReg
    // + add parentReg
    // + add shift:
    //   - if parentReg != DSP_REG_NONE, this is the shift where this
    //     register is found in the parentReg
    //   - if parentReg == DSP_REG_NONE, this is the current shift _state_
  };

  // Find a free host reg
  Gen::X64Reg FindFreeXReg() const;
  Gen::X64Reg SpillXReg();
  Gen::X64Reg FindSpillFreeXReg();
  void SpillXReg(Gen::X64Reg reg);

  void MovToHostReg(size_t reg, Gen::X64Reg host_reg, bool load);
  void MovToHostReg(size_t reg, bool load);
  void RotateHostReg(size_t reg, int shift, bool emit);
  void MovToMemory(size_t reg);
  void FlushMemBackedRegs();

  std::array<DynamicReg, 37> m_regs{};
  std::array<X64CachedReg, 16> m_xregs{};

  DSPEmitter& m_emitter;
  bool m_is_temporary;
  bool m_is_merged = false;

  int m_use_ctr = 0;
};

}  // namespace DSP::JIT::x64
