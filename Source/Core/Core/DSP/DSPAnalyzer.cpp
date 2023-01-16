// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/DSPAnalyzer.h"

#include <array>
#include <cstddef>

#include "Common/Logging/Log.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPTables.h"

namespace DSP
{
// Good candidates for idle skipping is mail wait loops. If we're time slicing
// between the main CPU and the DSP, if the DSP runs into one of these, it might
// as well give up its time slice immediately, after executing once.

// Max signature length is 6. A 0 in a signature is ignored.
constexpr size_t NUM_IDLE_SIGS = 8;
constexpr size_t MAX_IDLE_SIG_SIZE = 6;

// 0xFFFF means ignore.
constexpr u16 idle_skip_sigs[NUM_IDLE_SIGS][MAX_IDLE_SIG_SIZE + 1] = {
    // From AX:
    {0x26fc,          // LRS   $30, @DMBH
     0x02c0, 0x8000,  // ANDCF $30, #0x8000
     0x029d, 0xFFFF,  // JLZ 0x027a
     0, 0},           // RET
    {0x27fc,          // LRS   $31, @DMBH
     0x03c0, 0x8000,  // ANDCF $31, #0x8000
     0x029d, 0xFFFF,  // JLZ 0x027a
     0, 0},           // RET
    {0x26fe,          // LRS   $30, @CMBH
     0x02c0, 0x8000,  // ANDCF $30, #0x8000
     0x029c, 0xFFFF,  // JLNZ 0x0280
     0, 0},           // RET
    {0x27fe,          // LRS   $31, @CMBH
     0x03c0, 0x8000,  // ANDCF $31, #0x8000
     0x029c, 0xFFFF,  // JLNZ 0x0280
     0, 0},           // RET
    {0x26fc,          // LRS  $AC0.M, @DMBH
     0x02a0, 0x8000,  // ANDF $AC0.M, #0x8000
     0x029c, 0xFFFF,  // JLNZ 0x????
     0, 0},
    {0x27fc,          // LRS  $AC1.M, @DMBH
     0x03a0, 0x8000,  // ANDF $AC1.M, #0x8000
     0x029c, 0xFFFF,  // JLNZ 0x????
     0, 0},
    // From Zelda:
    {0x00de, 0xFFFE,  // LR    $AC0.M, @CMBH
     0x02c0, 0x8000,  // ANDCF $AC0.M, #0x8000
     0x029c, 0xFFFF,  // JLNZ 0x05cf
     0},
    // From Zelda - experimental
    {0x00da, 0x0352,  // LR     $AX0.H, @0x0352
     0x8600,          // TSTAXH $AX0.H
     0x0295, 0xFFFF,  // JZ    0x????
     0, 0},
};

Analyzer::Analyzer() = default;
Analyzer::~Analyzer() = default;

void Analyzer::Analyze(const SDSP& dsp)
{
  Reset();
  AnalyzeRange(dsp, 0x0000, 0x1000);  // IRAM
  AnalyzeRange(dsp, 0x8000, 0x9000);  // IROM
}

void Analyzer::Reset()
{
  m_code_flags.fill(0);
}

void Analyzer::AnalyzeRange(const SDSP& dsp, u16 start_addr, u16 end_addr)
{
  // First we run an extremely simplified version of a disassembler to find
  // where all instructions start.
  FindInstructionStarts(dsp, start_addr, end_addr);

  // Next, we'll scan for potential idle skips.
  FindIdleSkips(dsp, start_addr, end_addr);

  INFO_LOG_FMT(DSPLLE, "Finished analysis.");
}

void Analyzer::FindInstructionStarts(const SDSP& dsp, u16 start_addr, u16 end_addr)
{
  // This may not be 100% accurate in case of jump tables!
  // It could get desynced, which would be bad. We'll see if that's an issue.
#ifndef DISABLE_UPDATE_SR_ANALYSIS
  u16 last_arithmetic = 0;
#endif
  for (u16 addr = start_addr; addr < end_addr;)
  {
    const UDSPInstruction inst = dsp.ReadIMEM(addr);
    const DSPOPCTemplate* opcode = GetOpTemplate(inst);
    if (!opcode)
    {
      addr++;
      continue;
    }
    m_code_flags[addr] |= CODE_START_OF_INST;
    // Look for loops.
    if ((inst & 0xffe0) == 0x0060 || (inst & 0xff00) == 0x1100)
    {
      // BLOOP, BLOOPI
      const u16 loop_end = dsp.ReadIMEM(addr + 1);
      m_code_flags[addr] |= CODE_LOOP_START;
      m_code_flags[loop_end] |= CODE_LOOP_END;
    }
    else if ((inst & 0xffe0) == 0x0040 || (inst & 0xff00) == 0x1000)
    {
      // LOOP, LOOPI
      m_code_flags[addr] |= CODE_LOOP_START;
      m_code_flags[static_cast<u16>(addr + 1u)] |= CODE_LOOP_END;
    }

#ifndef DISABLE_UPDATE_SR_ANALYSIS
    // Mark the last arithmetic/multiplier instruction before a branch.
    // We must update the SR reg at these instructions
    if (opcode->updates_sr)
    {
      last_arithmetic = addr;
    }

    if (opcode->branch && !opcode->uncond_branch)
    {
      m_code_flags[last_arithmetic] |= CODE_UPDATE_SR;
    }
#endif

    // If an instruction potentially raises exceptions, mark the following instruction as needing to
    // check for exceptions.
    // Reads could trigger an accelerator address overflow exception
    const bool is_read = opcode->opcode == 0x00c0 ||  // LR
                         opcode->opcode == 0x1800 ||  // LRR
                         opcode->opcode == 0x1880 ||  // LRRD
                         opcode->opcode == 0x1900 ||  // LRRI
                         opcode->opcode == 0x1980 ||  // LRRN
                         opcode->opcode == 0x2000 ||  // LRS
                         opcode->extended;
    // Writes could also trigger an accelerator address overflow exception
    // (but we don't implement this currently)
    const bool is_write = opcode->opcode == 0x1600 ||  // SI
                          opcode->opcode == 0x00e0 ||  // SR
                          opcode->opcode == 0x1a00 ||  // SRR
                          opcode->opcode == 0x1a80 ||  // SRRD
                          opcode->opcode == 0x1b00 ||  // SRRI
                          opcode->opcode == 0x1b80 ||  // SRRN
                          opcode->opcode == 0x2800 ||  // SRS
                          opcode->opcode == 0x2c00 ||  // SRSH
                          opcode->extended;
    // Stack overflows trigger an exception (not emulated currently).
    // TODO: What happens if an exception saving to the stack overflows the stack?
    // I assume also that LOOP and LOOPI don't directly use the stack since they only run one
    // instruction.
    const bool pushes_stack = (opcode->opcode & 0xfff0) == 0x02b0 ||  // CALLcc
                              (opcode->opcode & 0xfff0) == 0x1710 ||  // CALLRcc
                              opcode->opcode == 0x0060 ||             // BLOOP
                              opcode->opcode == 0x1100;               // BLOOPI
    // Writes to one of the stack registers also pushes to the stack.
    // Instructions for is_read are not listed here.
    // $st0 through $st3 are 0x0c through 0x0f, so we can ignore the bottom 2 bits.
    const bool pushes_stack_directly = (inst & 0xfffc) == 0x008c ||  // LRI
                                       (inst & 0xff80) == 0x1d80;    // MRR
    // Stack underflows trigger an exception (not emulated currently).
    // Note that the end of a loop doesn't need to be counted here (if the stack's empty, there is
    // no end of the loop).
    const bool pops_stack = (opcode->opcode & 0xfff0) == 0x02d0 ||  // RETcc
                            (opcode->opcode & 0xfff0) == 0x02f0;    // RTIcc
    // Reads from one of the stack registers also pops the stack.
    // Instructions for is_write are not listed here.
    // The loop instructions can also read an arbitrary register (both BLOOP and LOOP),
    // including the stacks, though I don't know what happens on real hardware in that case.
    const bool pops_stack_directly = (opcode->opcode & 0xfc1c) == 0x1c0c ||  // MRR
                                     (opcode->opcode & 0xfffc) == 0x004c ||  // LOOP
                                     (opcode->opcode & 0xfffc) == 0x006c;    // BLOOP
    // If an instruction can change one of the interrupt enable bits in $sr, this can cause
    // a pending interrupt to fire. This only applies for direct loads/stores of $sr, and
    // some of the $sr bit instructions.
    // Instructions for is_read are not listed here, nor is RTI.
    // $sr is register 0x13.
    // SBSET and SBCLR add 6 to their bit, so 3 through 6 correspond to bits 9 through 12 of $sr
    // (IE, ?, EIE, ?).
    const bool updates_sr_exception = (inst & 0xffe0) == 0x1e60 ||           // MRR
                                      (inst >= 0x1203 && inst <= 0x1206) ||  // SBSET
                                      (inst >= 0x1303 && inst <= 0x1306);    // SBCLR
    if (is_read || is_write || pushes_stack || pushes_stack_directly || pops_stack ||
        pops_stack_directly || updates_sr_exception)
    {
      // TODO: Is this appropriate for CALL and RET?
      m_code_flags[static_cast<u16>(addr + opcode->size)] |= CODE_CHECK_EXC;
    }

    addr += opcode->size;
  }
}

void Analyzer::FindIdleSkips(const SDSP& dsp, u16 start_addr, u16 end_addr)
{
  for (size_t s = 0; s < NUM_IDLE_SIGS; s++)
  {
    for (u16 addr = start_addr; addr < end_addr; addr++)
    {
      bool found = false;
      for (size_t i = 0; i < MAX_IDLE_SIG_SIZE + 1; i++)
      {
        if (idle_skip_sigs[s][i] == 0)
          found = true;
        if (idle_skip_sigs[s][i] == 0xFFFF)
          continue;
        if (idle_skip_sigs[s][i] != dsp.ReadIMEM(static_cast<u16>(addr + i)))
          break;
      }
      if (found)
      {
        INFO_LOG_FMT(DSPLLE, "Idle skip location found at {:02x} (sigNum:{})", addr, s + 1);
        m_code_flags[addr] |= CODE_IDLE_SKIP;
      }
    }
  }
}
}  // namespace DSP
