// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/CPUThreadConfigCallback.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/ConditionRegister.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCCache.h"
#include "Core/PowerPC/PPCSymbolDB.h"

class CPUCoreBase;
class PointerWrap;
namespace CoreTiming
{
struct EventType;
}

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

enum class CoreMode
{
  Interpreter,
  JIT,
};

// TLB cache
constexpr size_t TLB_SIZE = 128;
constexpr size_t NUM_TLBS = 2;
constexpr size_t TLB_WAYS = 2;
constexpr size_t DATA_TLB_INDEX = 0;
constexpr size_t INST_TLB_INDEX = 1;

struct TLBEntry
{
  using WayArray = std::array<u32, TLB_WAYS>;

  static constexpr u32 INVALID_TAG = 0xffffffff;

  WayArray tag{INVALID_TAG, INVALID_TAG};
  WayArray paddr{};
  WayArray vsid{};
  WayArray pte{};
  u32 recent = 0;

  void Invalidate() { tag.fill(INVALID_TAG); }
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
//
// To minimize code size on x86, we want as much useful stuff in the first 256 bytes as possible.
// ps needs to be relatively late in the struct due to it being larger than 256 bytes in itself.
//
// On AArch64, most load/store instructions support fairly large immediate offsets,
// but not LDP/STP, which we want to use for accessing certain things.
// These must be in the first 260 bytes: pc, npc
// These must be in the first 520 bytes: gather_pipe_ptr, gather_pipe_base_ptr
// Better code is generated if these are in the first 260 bytes: gpr
// Better code is generated if these are in the first 520 bytes: ps
// Unfortunately not all of those fit in 520 bytes, but we can fit most of ps and all of the rest.
struct PowerPCState
{
  u32 pc = 0;  // program counter
  u32 npc = 0;

  // gather pipe pointer for JIT access
  u8* gather_pipe_ptr = nullptr;
  u8* gather_pipe_base_ptr = nullptr;

  u32 gpr[32]{};  // General purpose registers. r1 = stack pointer.

#ifndef _M_X86_64
  // The paired singles are strange : PS0 is stored in the full 64 bits of each FPR
  // but ps calculations are only done in 32-bit precision, and PS1 is only 32 bits.
  // Since we want to use SIMD, SSE2 is the only viable alternative - 2x double.
  alignas(16) PairedSingle ps[32];
#endif

  ConditionRegister cr{};

  UReg_MSR msr;      // machine state register
  UReg_FPSCR fpscr;  // floating point flags/status bits

  CPUEmuFeatureFlags feature_flags;

  // Exception management.
  u32 Exceptions = 0;

  // Downcount for determining when we need to do timing
  // This isn't quite the right location for it, but it is here to accelerate the ARM JIT
  // This variable should be inside of the CoreTiming namespace if we wanted to be correct.
  int downcount = 0;

  // XER, reformatted into byte fields for easier access.
  u8 xer_ca = 0;
  u8 xer_so_ov = 0;  // format: (SO << 1) | OV
  // The Broadway CPU implements bits 16-23 of the XER register... even though it doesn't support
  // lscbx
  u16 xer_stringctrl = 0;

#ifdef _M_X86_64
  // This member exists only for the purpose of an assertion that its offset <= 0x100.
  std::tuple<> above_fits_in_first_0x100;

  alignas(16) PairedSingle ps[32];
#endif

  u32 sr[16]{};  // Segment registers.

  // special purpose registers - controls quantizers, DMA, and lots of other misc extensions.
  // also for power management, but we don't care about that.
  // JitArm64 needs 64-bit alignment for SPR_TL.
  alignas(8) u32 spr[1024]{};

  // Storage for the stack pointer of the BLR optimization.
  u8* stored_stack_pointer = nullptr;
  u8* mem_ptr = nullptr;

  std::array<std::array<TLBEntry, TLB_SIZE / TLB_WAYS>, NUM_TLBS> tlb;

  u32 pagetable_base = 0;
  u32 pagetable_hashmask = 0;

  InstructionCache iCache;
  bool m_enable_dcache = false;
  Cache dCache;

  // Reservation monitor for lwarx and its friend stwcxd.
  bool reserve;
  u32 reserve_address;

  void UpdateCR1()
  {
    cr.SetField(1, (fpscr.FX << 3) | (fpscr.FEX << 2) | (fpscr.VX << 1) | fpscr.OX);
  }

  void SetSR(u32 index, u32 value);

  void SetCarry(u32 ca) { xer_ca = ca; }

  u32 GetCarry() const { return xer_ca; }

  UReg_XER GetXER() const
  {
    u32 xer = 0;
    xer |= xer_stringctrl;
    xer |= xer_ca << XER_CA_SHIFT;
    xer |= xer_so_ov << XER_OV_SHIFT;
    return UReg_XER{xer};
  }

  void SetXER(UReg_XER new_xer)
  {
    xer_stringctrl = new_xer.BYTE_COUNT + (new_xer.BYTE_CMP << 8);
    xer_ca = new_xer.CA;
    xer_so_ov = (new_xer.SO << 1) + new_xer.OV;
  }

  u32 GetXER_SO() const { return xer_so_ov >> 1; }

  void SetXER_SO(bool value) { xer_so_ov |= static_cast<u32>(value) << 1; }

  u32 GetXER_OV() const { return xer_so_ov & 1; }

  void SetXER_OV(bool value)
  {
    xer_so_ov = (xer_so_ov & 0xFE) | static_cast<u32>(value);
    SetXER_SO(value);
  }

  void UpdateFPRFDouble(double dvalue);
  void UpdateFPRFSingle(float fvalue);
};

#ifdef _M_X86_64
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(offsetof(PowerPC::PowerPCState, above_fits_in_first_0x100) <= 0x100,
              "top of PowerPCState too big");
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif

std::span<const CPUCore> AvailableCPUCores();
CPUCore DefaultCPUCore();

class PowerPCManager
{
public:
  explicit PowerPCManager(Core::System& system);
  PowerPCManager(const PowerPCManager& other) = delete;
  PowerPCManager(PowerPCManager&& other) = delete;
  PowerPCManager& operator=(const PowerPCManager& other) = delete;
  PowerPCManager& operator=(PowerPCManager&& other) = delete;
  ~PowerPCManager();

  void Init(CPUCore cpu_core);
  void Reset();
  void Shutdown();
  void DoState(PointerWrap& p);
  void ScheduleInvalidateCacheThreadSafe(u32 address);

  CoreMode GetMode() const;
  // [NOT THREADSAFE] CPU Thread or CPU::PauseAndLock or Core::State::Uninitialized
  void SetMode(CoreMode _coreType);
  const char* GetCPUName() const;

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

  u64 ReadFullTimeBaseValue() const;
  void WriteFullTimeBaseValue(u64 value);

  PowerPCState& GetPPCState() { return m_ppc_state; }
  const PowerPCState& GetPPCState() const { return m_ppc_state; }
  BreakPoints& GetBreakPoints() { return m_breakpoints; }
  const BreakPoints& GetBreakPoints() const { return m_breakpoints; }
  MemChecks& GetMemChecks() { return m_memchecks; }
  const MemChecks& GetMemChecks() const { return m_memchecks; }
  PPCDebugInterface& GetDebugInterface() { return m_debug_interface; }
  const PPCDebugInterface& GetDebugInterface() const { return m_debug_interface; }
  PPCSymbolDB& GetSymbolDB() { return m_symbol_db; }
  const PPCSymbolDB& GetSymbolDB() const { return m_symbol_db; }
  Core::BranchWatch& GetBranchWatch() { return m_branch_watch; }
  const Core::BranchWatch& GetBranchWatch() const { return m_branch_watch; }

private:
  void InitializeCPUCore(CPUCore cpu_core);
  void ApplyMode();
  void ResetRegisters();
  void RefreshConfig();

  PowerPCState m_ppc_state;

  CPUCoreBase* m_cpu_core_base = nullptr;
  bool m_cpu_core_base_is_injected = false;
  CoreMode m_mode = CoreMode::Interpreter;

  BreakPoints m_breakpoints;
  MemChecks m_memchecks;
  PPCSymbolDB m_symbol_db;
  PPCDebugInterface m_debug_interface;
  Core::BranchWatch m_branch_watch;

  CPUThreadConfigCallback::ConfigChangedCallbackID m_registered_config_callback_id;

  CoreTiming::EventType* m_invalidate_cache_thread_safe = nullptr;

  Core::System& m_system;
};

void UpdatePerformanceMonitor(u32 cycles, u32 num_load_stores, u32 num_fp_inst,
                              PowerPCState& ppc_state);

void CheckExceptionsFromJIT(PowerPCManager& power_pc);
void CheckExternalExceptionsFromJIT(PowerPCManager& power_pc);
void CheckBreakPointsFromJIT(PowerPCManager& power_pc);

// Easy register access macros.
#define HID0(ppc_state) ((UReg_HID0&)(ppc_state).spr[SPR_HID0])
#define HID2(ppc_state) ((UReg_HID2&)(ppc_state).spr[SPR_HID2])
#define HID4(ppc_state) ((UReg_HID4&)(ppc_state).spr[SPR_HID4])
#define DMAU(ppc_state) (*(UReg_DMAU*)&(ppc_state).spr[SPR_DMAU])
#define DMAL(ppc_state) (*(UReg_DMAL*)&(ppc_state).spr[SPR_DMAL])
#define MMCR0(ppc_state) ((UReg_MMCR0&)(ppc_state).spr[SPR_MMCR0])
#define MMCR1(ppc_state) ((UReg_MMCR1&)(ppc_state).spr[SPR_MMCR1])
#define THRM1(ppc_state) ((UReg_THRM12&)(ppc_state).spr[SPR_THRM1])
#define THRM2(ppc_state) ((UReg_THRM12&)(ppc_state).spr[SPR_THRM2])
#define THRM3(ppc_state) ((UReg_THRM3&)(ppc_state).spr[SPR_THRM3])

#define LR(ppc_state) (ppc_state).spr[SPR_LR]
#define CTR(ppc_state) (ppc_state).spr[SPR_CTR]
#define SRR0(ppc_state) (ppc_state).spr[SPR_SRR0]
#define SRR1(ppc_state) (ppc_state).spr[SPR_SRR1]
#define GQR(ppc_state, x) (ppc_state).spr[SPR_GQR0 + (x)]
#define TL(ppc_state) (ppc_state).spr[SPR_TL]
#define TU(ppc_state) (ppc_state).spr[SPR_TU]

void RoundingModeUpdated(PowerPCState& ppc_state);
void MSRUpdated(PowerPCState& ppc_state);
void MMCRUpdated(PowerPCState& ppc_state);
void RecalculateAllFeatureFlags(PowerPCState& ppc_state);

}  // namespace PowerPC
