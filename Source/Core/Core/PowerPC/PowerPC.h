// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCCache.h"

class CPUCoreBase;
class PointerWrap;

namespace PowerPC
{
// The gaps in the CPUCore numbering are from cores that only existed in the past.
// We avoid re-numbering cores so that settings will be compatible across versions.
enum CPUCore
{
  CORE_INTERPRETER = 0,
  CORE_JIT64 = 1,
  CORE_JITARM64 = 4,
  CORE_CACHEDINTERPRETER = 5,
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

struct TLBEntry
{
  static constexpr u32 INVALID_TAG = 0xffffffff;

  u32 tag[TLB_WAYS] = {INVALID_TAG, INVALID_TAG};
  u32 paddr[TLB_WAYS] = {};
  u32 pte[TLB_WAYS] = {};
  u8 recent = 0;
};

// This contains the entire state of the emulated PowerPC "Gekko" CPU.
struct GC_ALIGNED64(PowerPCState)
{
  u32 gpr[32];  // General purpose registers. r1 = stack pointer.

  u32 pc;  // program counter
  u32 npc;

  // Optimized CR implementation. Instead of storing CR in its PowerPC format
  // (4 bit value, SO/EQ/LT/GT), we store instead a 64 bit value for each of
  // the 8 CR register parts. This 64 bit value follows this format:
  //   - SO iff. bit 61 is set
  //   - EQ iff. lower 32 bits == 0
  //   - GT iff. (s64)cr_val > 0
  //   - LT iff. bit 62 is set
  //
  // This has the interesting property that sign-extending the result of an
  // operation from 32 to 64 bits results in a 64 bit value that works as a
  // CR value. Checking each part of CR is also fast, as it is equivalent to
  // testing one bit or the low 32 bit part of a register. And CR can still
  // be manipulated bit by bit fairly easily.
  u64 cr_val[8];

  u32 msr;    // machine specific register
  u32 fpscr;  // floating point flags/status bits

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
  GC_ALIGNED16(u64 ps[32][2]);

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
};

#if _M_X86_64
static_assert(offsetof(PowerPC::PowerPCState, above_fits_in_first_0x100) <= 0x100,
              "top of PowerPCState too big");
#endif

extern PowerPCState ppcState;

extern Watches watches;
extern BreakPoints breakpoints;
extern MemChecks memchecks;
extern PPCDebugInterface debug_interface;

const std::vector<CPUCore>& AvailableCPUCores();
CPUCore DefaultCPUCore();

void Init(int cpu_core);
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

u32 CompactCR();
void ExpandCR(u32 cr);

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
#define FPSCR ((UReg_FPSCR&)PowerPC::ppcState.fpscr)
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
#define GQR(x) PowerPC::ppcState.spr[SPR_GQR0 + x]
#define TL PowerPC::ppcState.spr[SPR_TL]
#define TU PowerPC::ppcState.spr[SPR_TU]

#define rPS0(i) (*(double*)(&PowerPC::ppcState.ps[i][0]))
#define rPS1(i) (*(double*)(&PowerPC::ppcState.ps[i][1]))

#define riPS0(i) (*(u64*)(&PowerPC::ppcState.ps[i][0]))
#define riPS1(i) (*(u64*)(&PowerPC::ppcState.ps[i][1]))

// Routines for debugger UI, cheats, etc. to access emulated memory from the
// perspective of the CPU.  Not for use by core emulation routines.
// Use "Host_" prefix.
u8 HostRead_U8(u32 address);
u16 HostRead_U16(u32 address);
u32 HostRead_U32(u32 address);
u64 HostRead_U64(u32 address);
u32 HostRead_Instruction(u32 address);

void HostWrite_U8(u8 var, u32 address);
void HostWrite_U16(u16 var, u32 address);
void HostWrite_U32(u32 var, u32 address);
void HostWrite_U64(u64 var, u32 address);

// Returns whether a read or write to the given address will resolve to a RAM
// access given the current CPU state.
bool HostIsRAMAddress(u32 address);
// Same as HostIsRAMAddress, but uses IBAT instead of DBAT.
bool HostIsInstructionRAMAddress(u32 address);

std::string HostGetString(u32 em_address, size_t size = 0);

// Routines for the CPU core to access memory.

// Used by interpreter to read instructions, uses iCache
u32 Read_Opcode(u32 address);
struct TryReadInstResult
{
  bool valid;
  bool from_bat;
  u32 hex;
  u32 physical_address;
};
TryReadInstResult TryReadInstruction(u32 address);

u8 Read_U8(u32 address);
u16 Read_U16(u32 address);
u32 Read_U32(u32 address);
u64 Read_U64(u32 address);

// Useful helper functions, used by ARM JIT
float Read_F32(u32 address);
double Read_F64(u32 address);

// used by JIT. Return zero-extended 32bit values
u32 Read_U8_ZX(u32 address);
u32 Read_U16_ZX(u32 address);

void Write_U8(u8 var, u32 address);
void Write_U16(u16 var, u32 address);
void Write_U32(u32 var, u32 address);
void Write_U64(u64 var, u32 address);

void Write_U16_Swap(u16 var, u32 address);
void Write_U32_Swap(u32 var, u32 address);
void Write_U64_Swap(u64 var, u32 address);

// Useful helper functions, used by ARM JIT
void Write_F64(double var, u32 address);

void DMA_LCToMemory(u32 memAddr, u32 cacheAddr, u32 numBlocks);
void DMA_MemoryToLC(u32 cacheAddr, u32 memAddr, u32 numBlocks);
void ClearCacheLine(u32 address);  // Zeroes 32 bytes; address should be 32-byte-aligned

// TLB functions
void SDRUpdated();
void InvalidateTLBEntry(u32 address);
void DBATUpdated();
void IBATUpdated();

// Result changes based on the BAT registers and MSR.DR.  Returns whether
// it's safe to optimize a read or write to this address to an unguarded
// memory access.  Does not consider page tables.
bool IsOptimizableRAMAddress(u32 address);
u32 IsOptimizableMMIOAccess(u32 address, u32 accessSize);
bool IsOptimizableGatherPipeWrite(u32 address);

struct TranslateResult
{
  bool valid;
  bool from_bat;
  u32 address;
};
TranslateResult JitCache_TranslateAddress(u32 address);

constexpr int BAT_INDEX_SHIFT = 17;
constexpr u32 BAT_PAGE_SIZE = 1 << BAT_INDEX_SHIFT;
constexpr u32 BAT_MAPPED_BIT = 0x1;
constexpr u32 BAT_PHYSICAL_BIT = 0x2;
constexpr u32 BAT_RESULT_MASK = UINT32_C(~0x3);
using BatTable = std::array<u32, 1 << (32 - BAT_INDEX_SHIFT)>;  // 128 KB
extern BatTable ibat_table;
extern BatTable dbat_table;
inline bool TranslateBatAddess(const BatTable& bat_table, u32* address)
{
  u32 bat_result = bat_table[*address >> BAT_INDEX_SHIFT];
  if ((bat_result & BAT_MAPPED_BIT) == 0)
    return false;
  *address = (bat_result & BAT_RESULT_MASK) | (*address & (BAT_PAGE_SIZE - 1));
  return true;
}
}  // namespace

enum CRBits
{
  CR_SO = 1,
  CR_EQ = 2,
  CR_GT = 4,
  CR_LT = 8,

  CR_SO_BIT = 0,
  CR_EQ_BIT = 1,
  CR_GT_BIT = 2,
  CR_LT_BIT = 3,
};

// Convert between PPC and internal representation of CR.
inline u64 PPCCRToInternal(u8 value)
{
  u64 cr_val = 0x100000000;
  cr_val |= (u64) !!(value & CR_SO) << 61;
  cr_val |= (u64) !(value & CR_EQ);
  cr_val |= (u64) !(value & CR_GT) << 63;
  cr_val |= (u64) !!(value & CR_LT) << 62;

  return cr_val;
}

// convert flags into 64-bit CR values with a lookup table
extern const std::array<u64, 16> m_crTable;

// Warning: these CR operations are fairly slow since they need to convert from
// PowerPC format (4 bit) to our internal 64 bit format. See the definition of
// ppcState.cr_val for more explanations.
inline void SetCRField(int cr_field, int value)
{
  PowerPC::ppcState.cr_val[cr_field] = m_crTable[value];
}

inline u32 GetCRField(int cr_field)
{
  u64 cr_val = PowerPC::ppcState.cr_val[cr_field];
  u32 ppc_cr = 0;

  // SO
  ppc_cr |= !!(cr_val & (1ull << 61));
  // EQ
  ppc_cr |= ((cr_val & 0xFFFFFFFF) == 0) << 1;
  // GT
  ppc_cr |= ((s64)cr_val > 0) << 2;
  // LT
  ppc_cr |= !!(cr_val & (1ull << 62)) << 3;

  return ppc_cr;
}

inline u32 GetCRBit(int bit)
{
  return (GetCRField(bit >> 2) >> (3 - (bit & 3))) & 1;
}

inline void SetCRBit(int bit, int value)
{
  if (value & 1)
    SetCRField(bit >> 2, GetCRField(bit >> 2) | (0x8 >> (bit & 3)));
  else
    SetCRField(bit >> 2, GetCRField(bit >> 2) & ~(0x8 >> (bit & 3)));
}

// SetCR and GetCR are fairly slow. Should be avoided if possible.
inline void SetCR(u32 new_cr)
{
  PowerPC::ExpandCR(new_cr);
}

inline u32 GetCR()
{
  return PowerPC::CompactCR();
}

inline void SetCarry(int ca)
{
  PowerPC::ppcState.xer_ca = ca;
}

inline int GetCarry()
{
  return PowerPC::ppcState.xer_ca;
}

inline UReg_XER GetXER()
{
  u32 xer = 0;
  xer |= PowerPC::ppcState.xer_stringctrl;
  xer |= PowerPC::ppcState.xer_ca << XER_CA_SHIFT;
  xer |= PowerPC::ppcState.xer_so_ov << XER_OV_SHIFT;
  return xer;
}

inline void SetXER(UReg_XER new_xer)
{
  PowerPC::ppcState.xer_stringctrl = new_xer.BYTE_COUNT + (new_xer.BYTE_CMP << 8);
  PowerPC::ppcState.xer_ca = new_xer.CA;
  PowerPC::ppcState.xer_so_ov = (new_xer.SO << 1) + new_xer.OV;
}

inline int GetXER_SO()
{
  return PowerPC::ppcState.xer_so_ov >> 1;
}

inline void SetXER_SO(int value)
{
  PowerPC::ppcState.xer_so_ov |= value << 1;
}

void UpdateFPRF(double dvalue);
