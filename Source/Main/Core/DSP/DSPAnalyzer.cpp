// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPAnalyzer.h"

#include <array>
#include <cstddef>

#include "Common/Logging/Log.h"

#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPTables.h"

namespace DSP
{
namespace Analyzer
{
namespace
{
constexpr size_t ISPACE = 65536;

// Holds data about all instructions in RAM.
std::array<u8, ISPACE> code_flags;

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
     0, 0}};

void Reset()
{
  code_flags.fill(0);
}

void AnalyzeRange(u16 start_addr, u16 end_addr)
{
  // First we run an extremely simplified version of a disassembler to find
  // where all instructions start.

  // This may not be 100% accurate in case of jump tables!
  // It could get desynced, which would be bad. We'll see if that's an issue.
  u16 last_arithmetic = 0;
  for (u16 addr = start_addr; addr < end_addr;)
  {
    UDSPInstruction inst = dsp_imem_read(addr);
    const DSPOPCTemplate* opcode = GetOpTemplate(inst);
    if (!opcode)
    {
      addr++;
      continue;
    }
    code_flags[addr] |= CODE_START_OF_INST;
    // Look for loops.
    if ((inst & 0xffe0) == 0x0060 || (inst & 0xff00) == 0x1100)
    {
      // BLOOP, BLOOPI
      u16 loop_end = dsp_imem_read(addr + 1);
      code_flags[addr] |= CODE_LOOP_START;
      code_flags[loop_end] |= CODE_LOOP_END;
    }
    else if ((inst & 0xffe0) == 0x0040 || (inst & 0xff00) == 0x1000)
    {
      // LOOP, LOOPI
      code_flags[addr] |= CODE_LOOP_START;
      code_flags[static_cast<u16>(addr + 1u)] |= CODE_LOOP_END;
    }

    // Mark the last arithmetic/multiplier instruction before a branch.
    // We must update the SR reg at these instructions
    if (opcode->updates_sr)
    {
      last_arithmetic = addr;
    }

    if (opcode->branch && !opcode->uncond_branch)
    {
      code_flags[last_arithmetic] |= CODE_UPDATE_SR;
    }

    // If an instruction potentially raises exceptions, mark the following
    // instruction as needing to check for exceptions
    if (opcode->opcode == 0x00c0 || opcode->opcode == 0x1800 || opcode->opcode == 0x1880 ||
        opcode->opcode == 0x1900 || opcode->opcode == 0x1980 || opcode->opcode == 0x2000 ||
        opcode->extended)
      code_flags[static_cast<u16>(addr + opcode->size)] |= CODE_CHECK_INT;

    addr += opcode->size;
  }

  // Next, we'll scan for potential idle skips.
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
        if (idle_skip_sigs[s][i] != dsp_imem_read(static_cast<u16>(addr + i)))
          break;
      }
      if (found)
      {
        INFO_LOG(DSPLLE, "Idle skip location found at %02x (sigNum:%zu)", addr, s + 1);
        code_flags[addr] |= CODE_IDLE_SKIP;
      }
    }
  }
  INFO_LOG(DSPLLE, "Finished analysis.");
}
}  // Anonymous namespace

void Analyze()
{
  Reset();
  AnalyzeRange(0x0000, 0x1000);  // IRAM
  AnalyzeRange(0x8000, 0x9000);  // IROM
}

u8 GetCodeFlags(u16 address)
{
  return code_flags[address];
}

}  // namespace Analyzer
}  // namespace DSP
