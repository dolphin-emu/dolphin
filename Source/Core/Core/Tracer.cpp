// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdlib>

#include "Common/Common.h"
#include "Common/FileUtil.h"

#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/Tracer.h"
#include "Core/PowerPC/PowerPC.h"

namespace Core {

static File::IOFile tracefile;

static bool bReadTrace = false;
static bool bWriteTrace = false;

void StartTrace(bool write)
{
	if (write)
	{
		tracefile.Open("L:\\trace.dat", "wb");
		bReadTrace = false;
		bWriteTrace = true;
	}
	else
	{
		tracefile.Open("L:\\trace.dat", "rb");
		bReadTrace = true;
		bWriteTrace = false;
	}
}

void StopTrace()
{
	tracefile.Close();
}

static int stateSize = 32*4;// + 32*16 + 6*4;

int SyncTrace()
{
	if (bWriteTrace)
	{
		tracefile.WriteBytes(&PowerPC::ppcState, stateSize);
		tracefile.Flush();
		return 1;
	}
	if (bReadTrace)
	{
		PowerPC::PowerPCState state;
		if (!tracefile.ReadBytes(&state, stateSize))
			return 1;

		bool difference = false;
		for (int i=0; i<32; i++)
		{
			if (PowerPC::ppcState.gpr[i] != state.gpr[i])
			{
				DEBUG_LOG(POWERPC, "DIFFERENCE - r%i (local %08x, remote %08x)", i, PowerPC::ppcState.gpr[i], state.gpr[i]);
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
		if (PowerPC::ppcState.npc != state.npc)
		{
			LOG(GEKKO, "DIFFERENCE - NPC (local %08x, remote %08x)", PowerPC::ppcState.npc, state.npc);
			difference = true;
		}
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

} // end of namespace Core
