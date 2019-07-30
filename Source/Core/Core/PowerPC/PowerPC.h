// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <iosfwd>
#include <tuple>
#include <type_traits>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/ConditionRegister.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCCache.h"

class CPUCoreBase;
class PointerWrap;

namespace PowerPC
{
// The gaps in the CPUCore numbering are from cores that only existed in the past.
// We avoid re-numbering cores so that settings will be compatible across versions.
enum class CPUCore
{
  Interpreter = 0,
  JIT64 = 1,
  JITARM64 = 4,
  CachedInterpreter = 5,
};

// For reading from and writing to our config.
std::istream& operator>>(std::istream& is, CPUCore& core);
std::ostream& operator<<(std::ostream& os, CPUCore core);

enum class CoreMode
{
  Interpreter,
  JIT,
};

// TLB cache
constexpr size_t TLB_SIZE = 128;
constexpr size_t NUM_TLBS = 2;
constexpr size_t TLB_WAYS = 2;

struct TLBEntry
{
  static constexpr u32 INVALID_TAG = 0xffffffff;

  u32 tag[TLB_WAYS] = {INVALID_TAG, INVALID_TAG};
  u32 paddr[TLB_WAYS] = {};
  u32 pte[TLB_WAYS] = {};
  u8 recent = 0;
};

struct PairedSingle
{
  u64 PS0AsU64() const { return ps0; }
  u64 PS1AsU64() const { return ps1; }

  u32 PS0AsU32() const { return static_cast<u32>(ps0); }
  u32 PS1AsU32() const { return static_cast<u32>(ps1); }

  double PS0AsDouble() const;
  double PS1AsDouble() const;

  void SetPS0(u64 value) { ps0 = value; }
  void SetPS0(double value);

  void SetPS1(u64 value) { ps1 = value; }
  void SetPS1(double value);

  void SetBoth(u64 lhs, u64 rhs)
  {
    SetPS0(lhs);
    SetPS1(rhs);
  }
  void SetBoth(double lhs, double rhs)
  {
    SetPS0(lhs);
    SetPS1(rhs);
  }

  void Fill(u64 value) { SetBoth(value, value); }
  void Fill(double value) { SetBoth(value, value); }

  u64 ps0 = 0;
  u64 ps1 = 0;
};
// Paired single must be standard layout in order for offsetof to work, which is used by the JITs
static_assert(std::is_standard_layout<PairedSingle>(), "PairedSingle must be standard layout");

// This contains the entire state of the emulated PowerPC "Gekko" CPU.
struct PowerPCState
{
  u32 gpr[32];  // General purpose registers. r1 = stack pointer.

  u32 pc;  // program counter
  u32 npc;

  ConditionRegister cr;

  UReg_MSR msr;      // machine state register
  UReg_FPSCR fpscr;  // floating point flags/status bits

  // Exception management.
  u32 Exceptions;

  // Downcount for determining when we need to do timing
  // This isn't quite the right location for it, but it is here to accelerate the ARM JIT
  // This variable should be inside of the CoreTiming namespace if we wanted to be correct.
  int downcount;

  // XER, reformatted into byte fields for easier access.
  u8 xer_ca;
  u8 xer_so_ov;  // format: (SO << 1) | OV
  // The Broadway CPU implements bits 16-23 of the XER register... even though it doesn't support
  // lscbx
  u16 xer_stringctrl;

  // gather pipe pointer for JIT access
  u8* gather_pipe_ptr;
  u8* gather_pipe_base_ptr;

#if _M_X86_64
  // This member exists for the purpose of an assertion in x86 JitBase.cpp
  // that its offset <= 0x100.  To minimize code size on x86, we want as much
  // useful stuff in the one-byte offset range as possible - which is why ps
  // is sitting down here.  It currently doesn't make a difference on other
  // supported architectures.
  std::tuple<> above_fits_in_first_0x100;
#endif

  // The paired singles are strange : PS0 is stored in the full 64 bits of each FPR
  // but ps calculations are only done in 32-bit precision, and PS1 is only 32 bits.
  // Since we want to use SIMD, SSE2 is the only viable alternative - 2x double.
  alignas(16) PairedSingle ps[32];

  u32 sr[16];  // Segment registers.

  // special purpose registers - controls quantizers, DMA, and lots of other misc extensions.
  // also for power management, but we don't care about that.
  u32 spr[1024];

  // Storage for the stack pointer of the BLR optimization.
  u8* stored_stack_pointer;

  std::array<std::array<TLBEntry, TLB_SIZE / TLB_WAYS>, NUM_TLBS> tlb;

  u32 pagetable_base;
  u32 pagetable_hashmask;

  InstructionCache iCache;

  void UpdateCR1()
  {
    cr.SetField(1, (fpscr.FX << 3) | (fpscr.FEX << 2) | (fpscr.VX << 1) | fpscr.OX);
  }

  void SetSR(u32 index, u32 value);
};

#if _M_X86_64
static_assert(offsetof(PowerPC::PowerPCState, above_fits_in_first_0x100) <= 0x100,
              "top of PowerPCState too big");
#endif

extern PowerPCState ppcState;

extern BreakPoints breakpoints;
extern MemChecks memchecks;
extern PPCDebugInterface debug_interface;

const std::vector<CPUCore>& AvailableCPUCores();
CPUCore DefaultCPUCore();

void Init(CPUCore cpu_core);
void Reset();
void Shutdown();
void DoState(PointerWrap& p);
void ScheduleInvalidateCacheThreadSafe(u32 address);

CoreMode GetMode();
// [NOT THREADSAFE] CPU Thread or CPU::PauseAndLock or Core::State::Uninitialized
void SetMode(CoreMode _coreType);
const char* GetCPUName();

// Set the current CPU Core to the given implementation until removed.
// Remove the current injected CPU Core by passing nullptr.
// While an external CPUCoreBase is injected, GetMode() will return CoreMode::Interpreter.
// Init() will be called when added and Shutdown() when removed.
// [Threadsafety: Same as SetMode(), except it cannot be called from inside the CPU
//  run loop on the CPU Thread - it doesn't make sense for a CPU to remove itself
//  while it is in State::Running]
void InjectExternalCPUCore(CPUCoreBase* core);

// Stepping requires the CPU Execution lock (CPU::PauseAndLock or CPU Thread)
// It's not threadsafe otherwise.
void SingleStep();
void CheckExceptions();
void CheckExternalExceptions();
void CheckBreakPoints();
void RunLoop();

u64 ReadFullTimeBaseValue();
void WriteFullTimeBaseValue(u64 value);

void UpdatePerformanceMonitor(u32 cycles, u32 num_load_stores, u32 num_fp_inst);

// Easy register access macros.
#define HID0 ((UReg_HID0&)PowerPC::ppcState.spr[SPR_HID0])
#define HID2 ((UReg_HID2&)PowerPC::ppcState.spr[SPR_HID2])
#define HID4 ((UReg_HID4&)PowerPC::ppcState.spr[SPR_HID4])
#define DMAU (*(UReg_DMAU*)&PowerPC::ppcState.spr[SPR_DMAU])
#define DMAL (*(UReg_DMAL*)&PowerPC::ppcState.spr[SPR_DMAL])
#define MMCR0 ((UReg_MMCR0&)PowerPC::ppcState.spr[SPR_MMCR0])
#define MMCR1 ((UReg_MMCR1&)PowerPC::ppcState.spr[SPR_MMCR1])
#define PC PowerPC::ppcState.pc
#define NPC PowerPC::ppcState.npc
#define FPSCR PowerPC::ppcState.fpscr
#define MSR PowerPC::ppcState.msr
#define GPR(n) PowerPC::ppcState.gpr[n]

#define rGPR PowerPC::ppcState.gpr
#define rSPR(i) PowerPC::ppcState.spr[i]
#define LR PowerPC::ppcState.spr[SPR_LR]
#define CTR PowerPC::ppcState.spr[SPR_CTR]
#define rDEC PowerPC::ppcState.spr[SPR_DEC]
#define SRR0 PowerPC::ppcState.spr[SPR_SRR0]
#define SRR1 PowerPC::ppcState.spr[SPR_SRR1]
#define SPRG0 PowerPC::ppcState.spr[SPR_SPRG0]
#define SPRG1 PowerPC::ppcState.spr[SPR_SPRG1]
#define SPRG2 PowerPC::ppcState.spr[SPR_SPRG2]
#define SPRG3 PowerPC::ppcState.spr[SPR_SPRG3]
#define GQR(x) PowerPC::ppcState.spr[SPR_GQR0 + (x)]
#define TL PowerPC::ppcState.spr[SPR_TL]
#define TU PowerPC::ppcState.spr[SPR_TU]

#define rPS(i) (PowerPC::ppcState.ps[(i)])

inline void SetCarry(u32 ca)
{
  PowerPC::ppcState.xer_ca = ca;
}

inline u32 GetCarry()
{
  return PowerPC::ppcState.xer_ca;
}

inline UReg_XER GetXER()
{
  u32 xer = 0;
  xer |= PowerPC::ppcState.xer_stringctrl;
  xer |= PowerPC::ppcState.xer_ca << XER_CA_SHIFT;
  xer |= PowerPC::ppcState.xer_so_ov << XER_OV_SHIFT;
  return UReg_XER{xer};
}

inline void SetXER(UReg_XER new_xer)
{
  PowerPC::ppcState.xer_stringctrl = new_xer.BYTE_COUNT + (new_xer.BYTE_CMP << 8);
  PowerPC::ppcState.xer_ca = new_xer.CA;
  PowerPC::ppcState.xer_so_ov = (new_xer.SO << 1) + new_xer.OV;
}

inline u32 GetXER_SO()
{
  return PowerPC::ppcState.xer_so_ov >> 1;
}

inline void SetXER_SO(bool value)
{
  PowerPC::ppcState.xer_so_ov |= static_cast<u32>(value) << 1;
}

inline u32 GetXER_OV()
{
  return PowerPC::ppcState.xer_so_ov & 1;
}

inline void SetXER_OV(bool value)
{
  PowerPC::ppcState.xer_so_ov = (PowerPC::ppcState.xer_so_ov & 0xFE) | static_cast<u32>(value);
  SetXER_SO(value);
}

void UpdateFPRF(double dvalue);

}  // namespace PowerPC
