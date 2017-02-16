// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

#include "Core/DSP/DSPBreakpoints.h"
#include "Core/DSP/DSPCaptureLogger.h"

namespace DSP
{
namespace JIT
{
namespace x86
{
class DSPEmitter;
}
}

enum : u32
{
  DSP_IRAM_BYTE_SIZE = 0x2000,
  DSP_IRAM_SIZE = 0x1000,
  DSP_IRAM_MASK = 0x0fff
};

enum : u32
{
  DSP_IROM_BYTE_SIZE = 0x2000,
  DSP_IROM_SIZE = 0x1000,
  DSP_IROM_MASK = 0x0fff
};

enum : u32
{
  DSP_DRAM_BYTE_SIZE = 0x2000,
  DSP_DRAM_SIZE = 0x1000,
  DSP_DRAM_MASK = 0x0fff
};

enum : u32
{
  DSP_COEF_BYTE_SIZE = 0x1000,
  DSP_COEF_SIZE = 0x800,
  DSP_COEF_MASK = 0x7ff
};

enum : u16
{
  DSP_RESET_VECTOR = 0x8000
};

enum : u8
{
  DSP_STACK_DEPTH = 0x20,
  DSP_STACK_MASK = 0x1f
};

enum : u32
{
  DSP_CR_IMEM = 2,
  DSP_CR_DMEM = 0,
  DSP_CR_TO_CPU = 1,
  DSP_CR_FROM_CPU = 0
};

// Register table taken from libasnd
enum : int
{
  // Address registers
  DSP_REG_AR0 = 0x00,
  DSP_REG_AR1 = 0x01,
  DSP_REG_AR2 = 0x02,
  DSP_REG_AR3 = 0x03,

  // Indexing registers (actually, mostly used as increments)
  DSP_REG_IX0 = 0x04,
  DSP_REG_IX1 = 0x05,
  DSP_REG_IX2 = 0x06,
  DSP_REG_IX3 = 0x07,

  // Address wrapping registers. should be initialized to 0xFFFF if not used.
  DSP_REG_WR0 = 0x08,
  DSP_REG_WR1 = 0x09,
  DSP_REG_WR2 = 0x0a,
  DSP_REG_WR3 = 0x0b,

  // Stacks
  DSP_REG_ST0 = 0x0c,
  DSP_REG_ST1 = 0x0d,
  DSP_REG_ST2 = 0x0e,
  DSP_REG_ST3 = 0x0f,

  // Seems to be the top 8 bits of LRS/SRS.
  DSP_REG_CR = 0x12,
  DSP_REG_SR = 0x13,

  // Product
  DSP_REG_PRODL = 0x14,
  DSP_REG_PRODM = 0x15,
  DSP_REG_PRODH = 0x16,
  DSP_REG_PRODM2 = 0x17,

  DSP_REG_AXL0 = 0x18,
  DSP_REG_AXL1 = 0x19,
  DSP_REG_AXH0 = 0x1a,
  DSP_REG_AXH1 = 0x1b,

  // Accumulator (global)
  DSP_REG_ACC0 = 0x1c,
  DSP_REG_ACC1 = 0x1d,

  DSP_REG_ACL0 = 0x1c,  // Low accumulator
  DSP_REG_ACL1 = 0x1d,
  DSP_REG_ACM0 = 0x1e,  // Mid accumulator
  DSP_REG_ACM1 = 0x1f,
  DSP_REG_ACH0 = 0x10,  // Sign extended 8 bit register 0
  DSP_REG_ACH1 = 0x11   // Sign extended 8 bit register 1
};

// Hardware registers address
enum : u32
{
  DSP_COEF_A1_0 = 0xa0,

  DSP_DSCR = 0xc9,   // DSP DMA Control Reg
  DSP_DSPA = 0xcd,   // DSP DMA Address (DSP)
  DSP_DSBL = 0xcb,   // DSP DMA Block Length
  DSP_DSMAH = 0xce,  // DSP DMA Address High (External)
  DSP_DSMAL = 0xcf,  // DSP DMA Address Low (External)

  DSP_FORMAT = 0xd1,   // Sample format
  DSP_ACUNK = 0xd2,    // Set to 3 on my dumps
  DSP_ACDATA1 = 0xd3,  // Used only by Zelda ucodes
  DSP_ACSAH = 0xd4,    // Start of loop
  DSP_ACSAL = 0xd5,
  DSP_ACEAH = 0xd6,  // End of sample (and loop)
  DSP_ACEAL = 0xd7,
  DSP_ACCAH = 0xd8,  // Current playback position
  DSP_ACCAL = 0xd9,
  DSP_PRED_SCALE = 0xda,  // ADPCM predictor and scale
  DSP_YN1 = 0xdb,
  DSP_YN2 = 0xdc,
  DSP_ACCELERATOR = 0xdd,  // ADPCM accelerator read. Used by AX.
  DSP_GAIN = 0xde,
  DSP_ACUNK2 = 0xdf,  // Set to 0xc on my dumps

  DSP_AMDM = 0xef,  // ARAM DMA Request Mask 0: DMA with ARAM unmasked 1: masked

  DSP_DIRQ = 0xfb,  // DSP Irq Rest
  DSP_DMBH = 0xfc,  // DSP Mailbox H
  DSP_DMBL = 0xfd,  // DSP Mailbox L
  DSP_CMBH = 0xfe,  // CPU Mailbox H
  DSP_CMBL = 0xff   // CPU Mailbox L
};

enum class StackRegister
{
  Call,
  Data,
  LoopAddress,
  LoopCounter
};

// cr (Not g_dsp.r[CR]) bits
// See HW/DSP.cpp.
enum : u32
{
  CR_EXTERNAL_INT = 0x0002,
  CR_HALT = 0x0004,
  CR_INIT = 0x0400
};

// SR bits
enum : u16
{
  SR_CARRY = 0x0001,
  SR_OVERFLOW = 0x0002,
  SR_ARITH_ZERO = 0x0004,
  SR_SIGN = 0x0008,
  SR_OVER_S32 = 0x0010,  // Set when there was mod/tst/cmp on accu and result is over s32
  SR_TOP2BITS = 0x0020,  // If the upper (ac?.m/ax?.h) 2 bits are equal
  SR_LOGIC_ZERO = 0x0040,
  SR_OVERFLOW_STICKY =
      0x0080,  // Set at the same time as 0x2 (under same conditions) - but not cleared the same
  SR_100 = 0x0100,         // Unknown
  SR_INT_ENABLE = 0x0200,  // Not 100% sure but duddie says so. This should replace the hack, if so.
  SR_400 = 0x0400,         // Unknown
  SR_EXT_INT_ENABLE = 0x0800,  // Appears in zelda - seems to disable external interrupts
  SR_1000 = 0x1000,            // Unknown
  SR_MUL_MODIFY = 0x2000,      // 1 = normal. 0 = x2   (M0, M2) (Free mul by 2)
  SR_40_MODE_BIT = 0x4000,     // 0 = "16", 1 = "40"  (SET16, SET40)  Controls sign extension when
                               // loading mid accums and data saturation for stores from mid accums.
  SR_MUL_UNSIGNED = 0x8000,    // 0 = normal. 1 = unsigned  (CLR15, SET15) If set, treats ax?.l as
                               // unsigned (MULX family only).

  // This should be the bits affected by CMP. Does not include logic zero.
  SR_CMP_MASK = 0x3f
};

// Exception vectors
enum : int
{
  EXP_STOVF = 1,  // 0x0002 stack under/over flow
  EXP_2 = 2,      // 0x0004
  EXP_3 = 3,      // 0x0006
  EXP_4 = 4,      // 0x0008
  EXP_ACCOV = 5,  // 0x000a accelerator address overflow
  EXP_6 = 6,      // 0x000c
  EXP_INT = 7     // 0x000e external int (message from CPU)
};

struct DSP_Regs
{
  u16 ar[4];
  u16 ix[4];
  u16 wr[4];
  u16 st[4];
  u16 cr;
  u16 sr;

  union
  {
    u64 val;
    struct
    {
      u16 l;
      u16 m;
      u16 h;
      u16 m2;  // if this gets in the way, drop it.
    };
  } prod;

  union
  {
    u32 val;
    struct
    {
      u16 l;
      u16 h;
    };
  } ax[2];

  union
  {
    u64 val;
    struct
    {
      u16 l;
      u16 m;
      u16 h;
    };
  } ac[2];
};

// All the state of the DSP should be in this struct. Any DSP state that is not filled on init
// should be moved here.
struct SDSP
{
  DSP_Regs r;
  u16 pc;
#if PROFILE
  u16 err_pc;
#endif

  // This is NOT the same cr as r.cr.
  // This register is shared with the main emulation, see DSP.cpp
  // The engine has control over 0x0C07 of this reg.
  // Bits are defined in a struct in DSP.cpp.
  u16 cr;

  u8 reg_stack_ptr[4];
  u8 exceptions;  // pending exceptions
  volatile bool external_interrupt_waiting;
  bool reset_dspjit_codespace;

  // DSP hardware stacks. They're mapped to a bunch of registers, such that writes
  // to them push and reads pop.
  // Let's make stack depth 32 for now, which is way more than what's needed.
  // The real DSP has different depths for the different stacks, but it would
  // be strange if any ucode relied on stack overflows since on the DSP, when
  // the stack overflows, you're screwed.
  u16 reg_stack[4][DSP_STACK_DEPTH];

  // For debugging.
  u32 iram_crc;
  u64 step_counter;

  // Mailbox.
  std::atomic<u32> mbox[2];

  // Accelerator / DMA / other hardware registers. Not GPRs.
  std::array<u16, 256> ifx_regs;

  // When state saving, all of the above can just be memcpy'd into the save state.
  // The below needs special handling.
  u16* iram;
  u16* dram;
  u16* irom;
  u16* coef;

  // This one doesn't really belong here.
  u8* cpu_ram;
};

extern SDSP g_dsp;
extern DSPBreakpoints g_dsp_breakpoints;
extern bool g_init_hax;
extern std::unique_ptr<JIT::x86::DSPEmitter> g_dsp_jit;
extern std::unique_ptr<DSPCaptureLogger> g_dsp_cap;

struct DSPInitOptions
{
  // DSP IROM blob, which is where the DSP boots from. Embedded into the DSP.
  std::array<u16, DSP_IROM_SIZE> irom_contents;

  // DSP DROM blob, which contains resampling coefficients.
  std::array<u16, DSP_COEF_SIZE> coef_contents;

  // Core used to emulate the DSP.
  // Default: CORE_JIT.
  enum CoreType
  {
    CORE_INTERPRETER,
    CORE_JIT,
  };
  CoreType core_type;

  // Optional capture logger used to log internal DSP data transfers.
  // Default: dummy implementation, does nothing.
  DSPCaptureLogger* capture_logger;

  DSPInitOptions() : core_type(CORE_JIT), capture_logger(new DefaultDSPCaptureLogger()) {}
};

// Initializes the DSP emulator using the provided options. Takes ownership of
// all the pointers contained in the options structure.
bool DSPCore_Init(const DSPInitOptions& opts);

void DSPCore_Reset();
void DSPCore_Shutdown();  // Frees all allocated memory.

void DSPCore_CheckExternalInterrupt();
void DSPCore_CheckExceptions();
void DSPCore_SetExternalInterrupt(bool val);

// sets a flag in the pending exception register.
void DSPCore_SetException(u8 level);

enum class State
{
  Stopped,
  Running,
  Stepping,
};

int DSPCore_RunCycles(int cycles);

// These are meant to be called from the UI thread.
void DSPCore_SetState(State new_state);
State DSPCore_GetState();

void DSPCore_Step();

u16 DSPCore_ReadRegister(size_t reg);
void DSPCore_WriteRegister(size_t reg, u16 val);
}  // namespace DSP
