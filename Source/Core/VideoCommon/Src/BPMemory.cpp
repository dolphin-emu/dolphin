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

#include "Common.h"
#include "BPMemory.h"

// BP state
// STATE_TO_SAVE
BPMemory bpmem;

// The plugin must implement this.
void BPWritten(const BPCmd& bp);

// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg()
void LoadBPReg(u32 value0)
{
	//handle the mask register
	int opcode = value0 >> 24;
	int oldval = ((u32*)&bpmem)[opcode];
	int newval = (oldval & ~bpmem.bpMask) | (value0 & bpmem.bpMask);
	int changes = (oldval ^ newval) & 0xFFFFFF;

	BPCmd bp = {opcode, changes, newval};

	//reset the mask register
	if (opcode != 0xFE)
		bpmem.bpMask = 0xFFFFFF;

	BPWritten(bp);
}

// Called when loading a saved state.
// Needs more testing though.
void BPReload()
{
	for (int i = 0; i < 254; i++)
	{
		switch (i) 
		{
		case BPMEM_BLENDMODE:
		case BPMEM_SETDRAWDONE:
		case BPMEM_TRIGGER_EFB_COPY:
		case BPMEM_LOADTLUT1:
		case BPMEM_PERF1:
		case BPMEM_PE_TOKEN_ID:
		case BPMEM_PE_TOKEN_INT_ID:
			// Cases in which we DON'T want to reload the BP
			continue;
		default:
			BPCmd bp = {i, 0xFFFFFF, ((u32*)&bpmem)[i]};
			BPWritten(bp);
		}
	}
}
