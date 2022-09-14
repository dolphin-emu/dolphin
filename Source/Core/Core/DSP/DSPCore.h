// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

#include "Common/Event.h"
#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPBreakpoints.h"
#include "Core/DSP/DSPCaptureLogger.h"

class PointerWrap;

namespace DSP
{
class Accelerator;
class DSPCore;

namespace Interpreter
{
class Interpreter;
}

namespace JIT
{
class DSPEmitter;
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
  DSP_REG_ACL0 = 0x1c,  // Low accumulator
  DSP_REG_ACL1 = 0x1d,
  DSP_REG_ACM0 = 0x1e,  // Mid accumulator
  DSP_REG_ACM1 = 0x1f,
  DSP_REG_ACH0 = 0x10,  // Sign extended 8 bit register 0
  DSP_REG_ACH1 = 0x11,  // Sign extended 8 bit register 1
};

enum : int
{
  // Magic values used by DSPTables.h
  // These do not correspond to real registers like above, but instead combined versions of the
  // registers.
  DSP_REG_ACC0_FULL = 0x20,
  DSP_REG_ACC1_FULL = 0x21,
  DSP_REG_AX0_FULL = 0x22,
  DSP_REG_AX1_FULL = 0x23,
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
  CR_RESET = 0x0001,
  CR_EXTERNAL_INT = 0x0002,
  CR_HALT = 0x0004,
  CR_INIT_CODE = 0x0400,
  CR_INIT = 0x0800
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
  SR_100 = 0x0100,         // Unknown, always reads back as 0
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
enum class ExceptionType
{
  StackOverflow = 1,        // 0x0002 stack under/over flow
  EXP_2 = 2,                // 0x0004
  EXP_3 = 3,                // 0x0006
  EXP_4 = 4,                // 0x0008
  AcceleratorOverflow = 5,  // 0x000a accelerator address overflow
  EXP_6 = 6,                // 0x000c
  ExternalInterrupt = 7     // 0x000e external int (message from CPU)
};

enum class Mailbox
{
  CPU,
  DSP
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
      u32 h;  // 32 bits so that val is fully sign-extended (only 8 bits are actually used)
    };
  } ac[2];
};

struct DSPInitOptions
{
  // DSP IROM blob, which is where the DSP boots from. Embedded into the DSP.
  std::array<u16, DSP_IROM_SIZE> irom_contents{};

  // DSP DROM blob, which contains resampling coefficients.
  std::array<u16, DSP_COEF_SIZE> coef_contents{};

  // Core used to emulate the DSP.
  // Default: JIT64.
  enum class CoreType
  {
    Interpreter,
    JIT64,
  };
  CoreType core_type = CoreType::JIT64;

  // Optional capture logger used to log internal DSP data transfers.
  // Default: dummy implementation, does nothing.
  DSPCaptureLogger* capture_logger;

  DSPInitOptions() : capture_logger(new DefaultDSPCaptureLogger()) {}
};

// All the state of the DSP should be in this struct. Any DSP state that is not filled on init
// should be moved here.
struct SDSP
{
  explicit SDSP(DSPCore& core);
  ~SDSP();

  SDSP(const SDSP&) = delete;
  SDSP& operator=(const SDSP&) = delete;

  SDSP(SDSP&&) = delete;
  SDSP& operator=(SDSP&&) = delete;

  // Initializes overall state.
  bool Initialize(const DSPInitOptions& opts);

  // Shuts down any necessary DSP state.
  void Shutdown();

  // Resets DSP state as if the reset exception vector has been taken.
  void Reset();

  // Initializes the IFX registers.
  void InitializeIFX();

  // Writes to IFX registers.
  void WriteIFX(u32 address, u16 value);

  // Reads from IFX registers.
  u16 ReadIFX(u16 address);

  // Checks the whole value within a mailbox.
  u32 PeekMailbox(Mailbox mailbox) const;

  // Reads the low part of the value in the specified mailbox.
  u16 ReadMailboxLow(Mailbox mailbox);

  // Reads the high part of the value in the specified mailbox.
  u16 ReadMailboxHigh(Mailbox mailbox);

  // Writes to the low part of the mailbox.
  void WriteMailboxLow(Mailbox mailbox, u16 value);

  // Writes to the high part of the mailbox.
  void WriteMailboxHigh(Mailbox mailbox, u16 value);

  // Reads from instruction memory.
  u16 ReadIMEM(u16 address) const;

  // Reads from data memory.
  u16 ReadDMEM(u16 address);

  // Write to data memory.
  void WriteDMEM(u16 address, u16 value);

  // Fetches the next instruction and increments the PC.
  u16 FetchInstruction();

  // Fetches the instruction at the PC address, but doesn't increment the PC.
  u16 PeekInstruction() const;

  // Skips over the next instruction in memory.
  void SkipInstruction();

  // Sets the given flags in the SR register.
  void SetSRFlag(u16 flag) { r.sr |= flag; }

  // Whether or not the given flag is set in the SR register.
  bool IsSRFlagSet(u16 flag) const { return (r.sr & flag) != 0; }

  // Indicates that a particular exception has occurred
  // and sets a flag in the pending exception register.
  void SetException(ExceptionType exception);

  // Checks if any exceptions occurred an updates the DSP state as appropriate.
  void CheckExceptions();

  // Notify that an external interrupt is pending (used by thread mode)
  void SetExternalInterrupt(bool val);

  // Coming from the CPU
  void CheckExternalInterrupt();

  // Stores a value into the specified stack
  void StoreStack(StackRegister stack_reg, u16 val);

  // Pops a value off of the specified stack
  u16 PopStack(StackRegister stack_reg);

  // Reads the current value from a particular register.
  u16 ReadRegister(size_t reg) const;

  // Writes a value to a given register.
  void WriteRegister(size_t reg, u16 val);

  // Advances the step counter used for debugging purposes.
  void AdvanceStepCounter() { ++m_step_counter; }

  // Sets the calculated IRAM CRC for debugging purposes.
  void SetIRAMCRC(u32 crc) { m_iram_crc = crc; }

  // Saves and loads any necessary state.
  void DoState(PointerWrap& p);

  // DSP static analyzer.
  Analyzer& GetAnalyzer() { return m_analyzer; }
  const Analyzer& GetAnalyzer() const { return m_analyzer; }

  DSP_Regs r{};
  u16 pc = 0;

  // This is NOT the same as r.cr.
  // This register is shared with the main emulation, see DSP.cpp
  // The engine has control over 0x0C07 of this reg.
  // Bits are defined in a struct in DSP.cpp.
  u16 control_reg = 0;
  u64 control_reg_init_code_clear_time = 0;

  u8 reg_stack_ptrs[4]{};
  u8 exceptions = 0;  // pending exceptions
  std::atomic<bool> external_interrupt_waiting = false;
  bool reset_dspjit_codespace = false;

  // DSP hardware stacks. They're mapped to a bunch of registers, such that writes
  // to them push and reads pop.
  // Let's make stack depth 32 for now, which is way more than what's needed.
  // The real DSP has different depths for the different stacks, but it would
  // be strange if any ucode relied on stack overflows since on the DSP, when
  // the stack overflows, you're screwed.
  u16 reg_stacks[4][DSP_STACK_DEPTH]{};

  // When state saving, all of the above can just be memcpy'd into the save state.
  // The below needs special handling.
  u16* iram = nullptr;
  u16* dram = nullptr;
  u16* irom = nullptr;
  u16* coef = nullptr;

private:
  auto& GetMailbox(Mailbox mailbox) { return m_mailbox[static_cast<u32>(mailbox)]; }
  const auto& GetMailbox(Mailbox mailbox) const { return m_mailbox[static_cast<u32>(mailbox)]; }

  void FreeMemoryPages();

  void DoDMA();
  const u8* DDMAIn(u16 dsp_addr, u32 addr, u32 size);
  const u8* DDMAOut(u16 dsp_addr, u32 addr, u32 size);
  const u8* IDMAIn(u16 dsp_addr, u32 addr, u32 size);
  const u8* IDMAOut(u16 dsp_addr, u32 addr, u32 size);

  u16 ReadIFXImpl(u16 address);

  // For debugging.
  u32 m_iram_crc = 0;
  u64 m_step_counter = 0;

  // Accelerator / DMA / other hardware registers. Not GPRs.
  std::array<u16, 256> m_ifx_regs{};

  std::unique_ptr<Accelerator> m_accelerator;
  std::array<std::atomic<u32>, 2> m_mailbox;
  DSPCore& m_dsp_core;
  Analyzer m_analyzer;
};

enum class State
{
  Stopped,
  Running,
  Stepping,
};

class DSPCore
{
public:
  DSPCore();
  ~DSPCore();

  DSPCore(const DSPCore&) = delete;
  DSPCore& operator=(const DSPCore&) = delete;

  DSPCore(DSPCore&&) = delete;
  DSPCore& operator=(DSPCore&&) = delete;

  // Initializes the DSP emulator using the provided options. Takes ownership of
  // all the pointers contained in the options structure.
  bool Initialize(const DSPInitOptions& opts);

  // Shuts down the DSP core and cleans up any necessary state.
  void Shutdown();

  // Delegates to JIT or interpreter as appropriate.
  // Handle state changes and stepping.
  int RunCycles(int cycles);

  // Steps the DSP by a single instruction.
  void Step();

  // Resets DSP state as if the reset exception vector has been taken.
  void Reset();

  // Clears the DSP instruction RAM.
  void ClearIRAM();

  // Dictates whether or not the DSP is currently stopped, running or stepping
  // through instructions.
  void SetState(State new_state);

  // Retrieves the current execution state of the DSP.
  State GetState() const;

  // Indicates that a particular exception has occurred
  // and sets a flag in the pending exception register.
  void SetException(ExceptionType exception);

  // Notify that an external interrupt is pending (used by thread mode)
  void SetExternalInterrupt(bool val);

  // Coming from the CPU
  void CheckExternalInterrupt();

  // Checks if any exceptions occurred an updates the DSP state as appropriate.
  void CheckExceptions();

  // Reads the current value from a particular register.
  u16 ReadRegister(size_t reg) const;

  // Writes a value to a given register.
  void WriteRegister(size_t reg, u16 val);

  // Checks the value within a mailbox.
  u32 PeekMailbox(Mailbox mailbox) const;

  // Reads the low part of the specified mailbox register.
  u16 ReadMailboxLow(Mailbox mailbox);

  // Reads the high part of the specified mailbox register.
  u16 ReadMailboxHigh(Mailbox mailbox);

  // Writes to the low part of the mailbox register.
  void WriteMailboxLow(Mailbox mailbox, u16 value);

  // Writes to the high part of the mailbox register.
  void WriteMailboxHigh(Mailbox mailbox, u16 value);

  // Logs an IFX register read.
  void LogIFXRead(u16 address, u16 read_value);

  // Logs an IFX register write.
  void LogIFXWrite(u16 address, u16 written_value);

  // Logs a DMA operation
  void LogDMA(u16 control, u32 gc_address, u16 dsp_address, u16 length, const u8* data);

  // Whether or not the JIT has been created.
  bool IsJITCreated() const;

  // Writes or loads state for savestates.
  void DoState(PointerWrap& p);

  // Accessors for the DSP breakpoint facilities.
  DSPBreakpoints& BreakPoints() { return m_dsp_breakpoints; }
  const DSPBreakpoints& BreakPoints() const { return m_dsp_breakpoints; }

  SDSP& DSPState() { return m_dsp; }
  const SDSP& DSPState() const { return m_dsp; }

  Interpreter::Interpreter& GetInterpreter() { return *m_dsp_interpreter; }
  const Interpreter::Interpreter& GetInterpreter() const { return *m_dsp_interpreter; }

private:
  SDSP m_dsp;
  DSPBreakpoints m_dsp_breakpoints;
  State m_core_state = State::Stopped;
  bool m_init_hax = false;
  std::unique_ptr<Interpreter::Interpreter> m_dsp_interpreter;
  std::unique_ptr<JIT::DSPEmitter> m_dsp_jit;
  std::unique_ptr<DSPCaptureLogger> m_dsp_cap;
  Common::Event m_step_event;
};
}  // namespace DSP
