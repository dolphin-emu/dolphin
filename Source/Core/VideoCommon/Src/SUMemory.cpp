// Copyright (C) 2003-2008 Dolphin Project.

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

#include "SUMemory.h"

// SU state
// STATE_TO_SAVE
SUMemory sumem;

// The plugin must implement this.
void SUWritten(const BPCommand& bp);

// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadSUReg()
void LoadSUReg(u32 value0)
{
	//handle the mask register
	int opcode = value0 >> 24;
	int oldval = ((u32*)&sumem)[opcode];
	int newval = (oldval & ~sumem.suMask) | (value0 & sumem.suMask);
	int changes = (oldval ^ newval) & 0xFFFFFF;

	BPCommand bp = {opcode, changes, newval};

	//reset the mask register
	if (opcode != 0xFE)
		sumem.suMask = 0xFFFFFF;

	SUWritten(bp);
}

// Called when loading a saved state.
// Needs more testing though.
void SUReload()
{
	for (int i = 0; i < 254; i++)
	{
		switch (i) {
		case 0x41:
		case 0x45: //GXSetDrawDone
		case 0x52:
		case 0x65:
		case 0x67: // set gp metric?
		case SUMEM_PE_TOKEN_ID:
		case SUMEM_PE_TOKEN_INT_ID:
			// Cases in which we DON'T want to reload the BP
			continue;
		default:
			BPCommand bp = {i, 0xFFFFFF, ((u32*)&sumem)[i]};
			SUWritten(bp);
		}
	}
}
