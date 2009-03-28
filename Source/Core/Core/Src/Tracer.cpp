// Copyright (C) 2003-2009 Dolphin Project.

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

#include <stdio.h>
#include <stdlib.h>

#include "Common.h"
#include "Tracer.h"

#include "Host.h"

#include "PowerPC/PowerPC.h"

namespace Core {

FILE *tracefile;

bool bReadTrace = false;
bool bWriteTrace = false;

void StartTrace(bool write)
{
	if (write)
	{
		tracefile = fopen("L:\\trace.dat","wb");
		bReadTrace = false;
		bWriteTrace = true;
	}
	else
	{
		tracefile = fopen("L:\\trace.dat","rb");
		bReadTrace = true;
		bWriteTrace = false;
	}
}

void StopTrace()
{
	if (tracefile)
	{
		fclose(tracefile);
		tracefile = 0;
	}
}

static int stateSize = 32*4;// + 32*16 + 6*4;
static u64 tb;

int SyncTrace()
{
	if (bWriteTrace)
	{
		fwrite(&PowerPC::ppcState, stateSize, 1, tracefile);
		fflush(tracefile);
		return 1;
	}
	if (bReadTrace)
	{
		PowerPC::PowerPCState state;
		if (feof(tracefile))
		{
			return 1;
		}
		fread(&state, stateSize, 1, tracefile);
		bool difference = false;
		for (int i=0; i<32; i++)
		{
			if (PowerPC::ppcState.gpr[i] != state.gpr[i])
			{
				DEBUG_LOG(GEKKO, "DIFFERENCE - r%i (local %08x, remote %08x)", i, PowerPC::ppcState.gpr[i], state.gpr[i]);
				difference = true;
			}
		}
/*
		for (int i=0; i<32; i++)
		{
			for (int j=0; j<2; j++)
			{
				if (PowerPC::ppcState.ps[i][j] != state.ps[i][j])
				{
					LOG(GEKKO, "DIFFERENCE - ps%i_%i (local %f, remote %f)", i, j, PowerPC::ppcState.ps[i][j], state.ps[i][j]);
					difference = true;
				}
			}
		}*/
		/*
		if (GetCR() != state.cr)
		{
			LOG(GEKKO, "DIFFERENCE - CR (local %08x, remote %08x)", PowerPC::ppcState.cr, state.cr);
			difference = true;
		}
		if (PowerPC::ppcState.pc != state.pc)
		{
			LOG(GEKKO, "DIFFERENCE - PC (local %08x, remote %08x)", PowerPC::ppcState.pc, state.pc);
			difference = true;
		}
		//if (PowerPC::ppcState.npc != state.npc)
		///{
		//	LOG(GEKKO, "DIFFERENCE - NPC (local %08x, remote %08x)", PowerPC::ppcState.npc, state.npc);
		//	difference = true;
		//}
		if (PowerPC::ppcState.msr != state.msr)
		{
			LOG(GEKKO, "DIFFERENCE - MSR (local %08x, remote %08x)", PowerPC::ppcState.msr, state.msr);
			difference = true;
		}
		if (PowerPC::ppcState.fpscr != state.fpscr)
		{
			LOG(GEKKO, "DIFFERENCE - FPSCR (local %08x, remote %08x)", PowerPC::ppcState.fpscr, state.fpscr);
			difference = true;
		}
*/
		if (difference)
		{
			Host_UpdateLogDisplay();
			//Also show drec compare window here
			//CDynaViewDlg::Show(true);
			//CDynaViewDlg::ViewAddr(m_BlockStart);
			//CDynaViewDlg::Show(true);
			//PanicAlert("Hang on");
			//Sleep(INFINITE);
			return 0;
		}
		else
		{
			return 1;
			//LOG(GEKKO, "No difference!");
		}
	}
	return 1;

}
}

