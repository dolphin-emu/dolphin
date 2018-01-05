// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"

#include "Core/PowerPC/Jit64Common/ConstantPool.h"
#include "Core/PowerPC/Jit64Common/FarCodeCache.h"
#include "Core/PowerPC/Jit64Common/TrampolineInfo.h"

namespace MMIO
{
class Mapping;
}

// Like XCodeBlock but has some utilities for memory access.
class EmuCodeBlock : public Gen::X64CodeBlock
{
public:
  void MemoryExceptionCheck();

  // Simple functions to switch between near and far code emitting
  void SwitchToFarCode();
  void SwitchToNearCode();

  template <typename T>
  const void* GetConstantFromPool(const T& value)
  {
    return m_const_pool.GetConstant(&value, sizeof(T), 1, 0);
  }

  template <typename T>
  Gen::OpArg MConst(const T& value)
  {
    return Gen::M(GetConstantFromPool(value));
  }

  template <typename T, size_t N>
  Gen::OpArg MConst(const T (&value)[N], size_t index = 0)
  {
    return Gen::M(m_const_pool.GetConstant(&value, sizeof(T), N, index));
  }

  Gen::FixupBranch CheckIfSafeAddress(const Gen::OpArg& reg_value, Gen::X64Reg reg_addr,
                                      BitSet32 registers_in_use);
  void UnsafeLoadRegToReg(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize,
                          s32 offset = 0, bool signExtend = false);
  void UnsafeLoadRegToRegNoSwap(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize,
                                s32 offset, bool signExtend = false);
  // these return the address of the MOV, for backpatching
  void UnsafeWriteRegToReg(Gen::OpArg reg_value, Gen::X64Reg reg_addr, int accessSize,
                           s32 offset = 0, bool swap = true, Gen::MovInfo* info = nullptr);
  void UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize,
                           s32 offset = 0, bool swap = true, Gen::MovInfo* info = nullptr);

  bool UnsafeLoadToReg(Gen::X64Reg reg_value, Gen::OpArg opAddress, int accessSize, s32 offset,
                       bool signExtend, Gen::MovInfo* info = nullptr);
  void UnsafeWriteGatherPipe(int accessSize);

  // Generate a load/write from the MMIO handler for a given address. Only
  // call for known addresses in MMIO range (MMIO::IsMMIOAddress).
  void MMIOLoadToReg(MMIO::Mapping* mmio, Gen::X64Reg reg_value, BitSet32 registers_in_use,
                     u32 address, int access_size, bool sign_extend);

  enum SafeLoadStoreFlags
  {
    SAFE_LOADSTORE_NO_SWAP = 1,
    SAFE_LOADSTORE_NO_PROLOG = 2,
    // This indicates that the write being generated cannot be patched (and thus can't use fastmem)
    SAFE_LOADSTORE_NO_FASTMEM = 4,
    SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR = 8,
    // Force slowmem (used when generating fallbacks in trampolines)
    SAFE_LOADSTORE_FORCE_SLOWMEM = 16,
    SAFE_LOADSTORE_DR_ON = 32,
  };

  void SafeLoadToReg(Gen::X64Reg reg_value, const Gen::OpArg& opAddress, int accessSize, s32 offset,
                     BitSet32 registersInUse, bool signExtend, int flags = 0);
  void SafeLoadToRegImmediate(Gen::X64Reg reg_value, u32 address, int accessSize,
                              BitSet32 registersInUse, bool signExtend);

  // Clobbers RSCRATCH or reg_addr depending on the relevant flag.  Preserves
  // reg_value if the load fails and js.memcheck is enabled.
  // Works with immediate inputs and simple registers only.
  void SafeWriteRegToReg(Gen::OpArg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset,
                         BitSet32 registersInUse, int flags = 0);
  void SafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset,
                         BitSet32 registersInUse, int flags = 0);

  // applies to safe and unsafe WriteRegToReg
  bool WriteClobbersRegValue(int accessSize, bool swap);

  // returns true if an exception could have been caused
  bool WriteToConstAddress(int accessSize, Gen::OpArg arg, u32 address, BitSet32 registersInUse);
  void WriteToConstRamAddress(int accessSize, Gen::OpArg arg, u32 address, bool swap = true);

  void JitGetAndClearCAOV(bool oe);
  void JitSetCA();
  void JitSetCAIf(Gen::CCFlags conditionCode);
  void JitClearCA();

  void avx_op(void (Gen::XEmitter::*avxOp)(Gen::X64Reg, Gen::X64Reg, const Gen::OpArg&),
              void (Gen::XEmitter::*sseOp)(Gen::X64Reg, const Gen::OpArg&), Gen::X64Reg regOp,
              const Gen::OpArg& arg1, const Gen::OpArg& arg2, bool packed = true,
              bool reversible = false);
  void avx_op(void (Gen::XEmitter::*avxOp)(Gen::X64Reg, Gen::X64Reg, const Gen::OpArg&, u8),
              void (Gen::XEmitter::*sseOp)(Gen::X64Reg, const Gen::OpArg&, u8), Gen::X64Reg regOp,
              const Gen::OpArg& arg1, const Gen::OpArg& arg2, u8 imm);

  void ForceSinglePrecision(Gen::X64Reg output, const Gen::OpArg& input, bool packed = true,
                            bool duplicate = false);
  void Force25BitPrecision(Gen::X64Reg output, const Gen::OpArg& input, Gen::X64Reg tmp);

  // RSCRATCH might get trashed
  void ConvertSingleToDouble(Gen::X64Reg dst, Gen::X64Reg src, bool src_is_gpr = false);
  void ConvertDoubleToSingle(Gen::X64Reg dst, Gen::X64Reg src);
  void SetFPRF(Gen::X64Reg xmm);
  void Clear();

protected:
  ConstantPool m_const_pool;
  FarCodeCache m_far_code;
  u8* m_near_code;  // Backed up when we switch to far code.

  std::unordered_map<u8*, TrampolineInfo> m_back_patch_info;
  std::unordered_map<u8*, u8*> m_exception_handler_at_loc;
};
