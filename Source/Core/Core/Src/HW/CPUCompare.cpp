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
#ifdef _WIN32
#include <windows.h>
#endif

#include "Common.h"
#include "../Core.h"

#include "CPUCompare.h"
#include "../PowerPC/PowerPC.h"
#include "CommandProcessor.h"


#include "../Host.h"

#ifdef _WIN32

namespace CPUCompare
{

HANDLE m_hPipe;
bool   m_bIsServer;
bool   m_bEnabled;
u32    m_BlockStart;

#define PIPENAME "\\\\.\\pipe\\cpucompare"


int stateSize = 32*4 + 32*16 + 6*4;


void SetBlockStart(u32 addr)
{
	m_BlockStart = addr;
}

void StartServer()
{
	_assert_msg_(GEKKO, Core::GetStartupParameter().bUseDualCore != true, "Don't use multithreading together with CPU-compare.");
	
	if (m_bEnabled)
		return;
	//TODO: error checking
	m_hPipe = CreateNamedPipe(
		PIPENAME,
		PIPE_ACCESS_OUTBOUND,
		PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
		1, //maxinst
		0x1000, //outbufsize
		0x1000, //inbufsize
		INFINITE, //timeout
		0);
	_assert_msg_(GEKKO, m_hPipe != INVALID_HANDLE_VALUE, "Failed to create pipe.");
	//_assert_msg_(GEKKO, 0, "Pipe %s created.", PIPENAME);

	m_bIsServer = true;
	m_bEnabled = true;
}

void ConnectAsClient()
{
	_assert_msg_(GEKKO, Core::GetStartupParameter().bUseDualCore != true, "Don't use multithreading together with CPU-compare.");

	if (m_bEnabled)
		return;

	//TODO: error checking
	m_hPipe = CreateFile(
		PIPENAME,
		GENERIC_READ,
		0, //share
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	
	_assert_msg_(GEKKO, m_hPipe != INVALID_HANDLE_VALUE, "Failed to connect to pipe. %08x (2 = file not found)", GetLastError());

	m_bEnabled = true;
	m_bIsServer = false;
}

void Stop()
{
	if (m_bEnabled)
	{
		if (m_bIsServer)
		{
			DisconnectNamedPipe(m_hPipe);
			CloseHandle(m_hPipe); 
		}
		else
		{
			CloseHandle(m_hPipe); //both for server and client i guess
		}
		m_bEnabled=false;
	}
}

int Sync()
{
	_assert_msg_(GEKKO,0,"Sync - PC = %08x", PC);

	PowerPC::PowerPCState state;
	if (!m_bEnabled) 
		return 0;

	if (m_bIsServer) //This should be interpreter
	{
		//write cpu state to m_hPipe
		HRESULT result;
		u32 written;
	//	LogManager::Redraw();
		result = WriteFile(m_hPipe, &PowerPC::ppcState, stateSize, (LPDWORD)&written,FALSE);
		//_assert_msg_(GEKKO, 0, "Server Wrote!");
		if (FAILED(result))
		{
			_assert_msg_(GEKKO,0,"Failed to write cpu state to named pipe");
			Stop();
		}
	//	LogManager::Redraw();
	}
	else //This should be dynarec
	{
		u32 read;
		memset(&state,0xcc,stateSize);
		BOOL res = ReadFile(m_hPipe, &state, stateSize, (LPDWORD)&read, FALSE);
		//_assert_msg_(GEKKO, 0, "Client got data!");

		//read cpu state to m_hPipe and compare
		//if any errors, print report
		if (!res || read != stateSize)
		{
			_assert_msg_(GEKKO,0,"Failed to read cpu state from named pipe");
			Stop();
		}
		else
		{
			bool difference = false;
			for (int i=0; i<32; i++)
			{
				if (PowerPC::ppcState.gpr[i] != state.gpr[i])
				{
					LOG(GEKKO, "DIFFERENCE - r%i (local %08x, remote %08x)", i, PowerPC::ppcState.gpr[i], state.gpr[i]);
					difference = true;
				}
			}

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
			}
			if (PowerPC::ppcState.cr != state.cr)
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

			if (difference)
			{
				Host_UpdateLogDisplay();
				//Also show drec compare window here
				//CDynaViewDlg::Show(true);
				//CDynaViewDlg::ViewAddr(m_BlockStart);
				//CDynaViewDlg::Show(true);
				//Sleep(INFINITE);
				return false;
			}
			else
			{
				return true;
				//LOG(GEKKO, "No difference!");
			}
		}
	}
	return 0;
}

bool IsEnabled()
{
	return m_bEnabled;
}

bool IsServer()
{
	return m_bIsServer;
}


}
#else

namespace CPUCompare
{
void StartServer() { }
void ConnectAsClient() { }
void Stop() { }
int Sync() { return 0; }
bool IsEnabled() { return false; }
bool IsServer() { return false; }
}
// #error Provide a CPUCompare implementation or dummy it out, please

#endif
