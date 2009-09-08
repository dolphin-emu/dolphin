// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "DSPAnalyzer.h"
#include "DSPInterpreter.h"
#include "DSPTables.h"
#include "DSPMemoryMap.h"

namespace DSPAnalyzer {

// Holds data about all instructions in RAM.
u8 code_flags[ISPACE];

// Good candidates for idle skipping is mail wait loops. If we're time slicing
// between the main CPU and the DSP, if the DSP runs into one of these, it might
// as well give up its time slice immediately, after executing once.

// Max signature length is 6. A 0 in a signature is ignored.
#define NUM_IDLE_SIGS 5
#define MAX_IDLE_SIG_SIZE 6

// 0xFFFF means ignore.
const u16 idle_skip_sigs[NUM_IDLE_SIGS][MAX_IDLE_SIG_SIZE + 1] =
{
	// From AX:
	{ 0x26fc,          // LRS $30, @DMBH
	  0x02c0, 0x8000,  // ANDCF $30, #0x8000
	  0x029d, 0xFFFF,  // JLZ 0x027a
	  0, 0 },     // RET
	{ 0x27fc,          // LRS $31, @DMBH
	  0x03c0, 0x8000,  // ANDCF $31, #0x8000
	  0x029d, 0xFFFF,  // JLZ 0x027a
	  0, 0 },     // RET
	{ 0x26fe,          // LRS $30, @CMBH
	  0x02c0, 0x8000,  // ANDCF $30, #0x8000
	  0x029c, 0xFFFF,  // JLNZ 0x0280
	  0, 0 },     // RET
	{ 0x27fe,          // LRS $31, @CMBH
	  0x03c0, 0x8000,  // ANDCF $31, #0x8000
	  0x029c, 0xFFFF,  // JLNZ 0x0280
	  0, 0 },     // RET

	// From Zelda:
	{ 0x00de, 0xFFFE,  // LR $AC0.M, @CMBH
	  0x02c0, 0x8000,  // ANDCF $AC0.M, #0x8000 
	  0x029c, 0xFFFF,  // JLNZ 0x05cf
	  0 }
};

void Reset()
{
	memset(code_flags, 0, sizeof(code_flags));
}

void AnalyzeRange(int start_addr, int end_addr)
{
	// First we run an extremely simplified version of a disassembler to find
	// where all instructions start.

	// This may not be 100% accurate in case of jump tables!
	// It could get desynced, which would be bad. We'll see if that's an issue.
	for (int addr = start_addr; addr < end_addr;)
	{
		UDSPInstruction inst = dsp_imem_read(addr);
		const DSPOPCTemplate *opcode = GetOpTemplate(inst);
		if (!opcode)
		{
			addr++;
			continue;
		}
		code_flags[addr] |= CODE_START_OF_INST;
		addr += opcode->size;

		// Look for loops.
		if ((inst.hex & 0xffe0) == 0x0060 || (inst.hex & 0xff00) == 0x1100) {
			// BLOOP, BLOOPI
			u16 loop_end = dsp_imem_read(addr + 1);
			code_flags[loop_end] |= CODE_LOOP_END;
		} else if ((inst.hex & 0xffe0) == 0x0040 || (inst.hex & 0xff00) == 0x1000) {
			// LOOP, LOOPI
			code_flags[addr + 1] |= CODE_LOOP_END;
		}
	}

	// Next, we'll scan for potential idle skips.
	for (int s = 0; s < NUM_IDLE_SIGS; s++)
	{
		for (int addr = start_addr; addr < end_addr; addr++)
		{
			bool found = false;
			for (int i = 0; i < MAX_IDLE_SIG_SIZE + 1; i++)
			{
				if (idle_skip_sigs[s][i] == 0)
					found = true;
				if (idle_skip_sigs[s][i] == 0xFFFF)
					continue;
				if (idle_skip_sigs[s][i] != dsp_imem_read(addr + i))
					break;
			}
			if (found)
			{
				NOTICE_LOG(DSPLLE, "Idle skip location found at %02x", addr);
				code_flags[addr] |= CODE_IDLE_SKIP;
				// TODO: actually use this flag somewhere.
			}
		}
	}
	NOTICE_LOG(DSPLLE, "Finished analysis.");
}

void Analyze()
{
	Reset();
	AnalyzeRange(0x0000, 0x1000);  // IRAM
	AnalyzeRange(0x8000, 0x9000);  // IROM
}

}  // namespace
