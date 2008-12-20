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
#include "Thread.h"

#include "../PowerPC/PowerPC.h"
#include "../Host.h"
#include "../Core.h"
#include "CPU.h"
#include "CPUCompare.h"

#include "../Debugger/Debugger_BreakPoints.h"

using namespace PowerPC;
namespace {
	static bool g_Branch;
	static Common::Event m_StepEvent;
	static Common::Event *m_SyncEvent;
}

void CCPU::Init()
{
	m_StepEvent.Init();
	PowerPC::Init();
	m_SyncEvent = 0;
}

void CCPU::Shutdown()
{
	PowerPC::Shutdown();
	m_StepEvent.Shutdown();
	m_SyncEvent = 0;
}

void CCPU::Run()
{
	Host_UpdateDisasmDialog();

	while (true)
	{
		switch(PowerPC::state) {
		case CPU_RUNNING:
			//1: enter a fast runloop
			PowerPC::RunLoop();
			break;

		case CPU_RUNNINGDEBUG:
			//1: check for cpucompare
			/*
			if (CPUCompare::IsEnabled() && g_Branch)
			{
				g_Branch = false;
				CPUCompare::Sync();
			}*/

			//2: check for breakpoint
			if (BreakPoints::IsAddressBreakPoint(PC))
			{
				LOG(GEKKO, "Hit Breakpoint - %08x", PC);
				EnableStepping(true);
				if (BreakPoints::IsTempBreakPoint(PC))
					BreakPoints::Remove(PC);

                Host_UpdateDisasmDialog();

				break;                
			}

/*			if (!Core::g_CoreStartupParameter.bUseJIT && BreakPoints::GetBreakCount() == PowerPC::ppcState.DebugCount)
			{
				LOG(GEKKO, "Hit DebugCount breakpoint - %i", PowerPC::ppcState.DebugCount);
				EnableStepping(true);
				break;
			}*/

			//3: do a step
			PowerPC::SingleStep();
			break;

		case CPU_STEPPING:
			//1: wait for step command..

			m_StepEvent.Wait();
			if (state == CPU_POWERDOWN)
				return;
	
			//2: check for cpu compare
			if (CPUCompare::IsEnabled() && g_Branch)
			{
				g_Branch = false;
				CPUCompare::Sync();
			}

			//3: do a step
			PowerPC::SingleStep();

			//4: update disasm dialog
			if (m_SyncEvent) {
				m_SyncEvent->Set();
				m_SyncEvent = 0;
			}
			Host_UpdateDisasmDialog();
			break;

		case CPU_POWERDOWN:
			//1: Exit loop!!
			return; 
		}
	}
}

void CCPU::Stop()
{
	PowerPC::Stop();
	m_StepEvent.Set();
}

bool CCPU::IsStepping()
{
	return PowerPC::state == CPU_STEPPING;
}

void CCPU::Reset()
{

} 

void CCPU::StepOpcode(Common::Event *event) 
{
	m_StepEvent.Set();
	if (PowerPC::state == CPU_STEPPING)
	{
		m_SyncEvent = event;
	}
}

void CCPU::EnableStepping(const bool _bStepping)
{	
	if (_bStepping)
	{
		PowerPC::Pause();
		// TODO(ector): why a sleep?
		Host_SetDebugMode(true);
	}
	else
	{
		Host_SetDebugMode(false);
		PowerPC::Start();
		m_StepEvent.Set();
	}
}

void CCPU::SingleStep()
{
	switch (PowerPC::state) 
	{
		case CPU_RUNNING:
			//1: enter a fast runloop
			PowerPC::RunLoop();
			break;

		case CPU_RUNNINGDEBUG:
			//1: check for cpucompare
			if (CPUCompare::IsEnabled() && g_Branch)
			{
				g_Branch = false;
				CPUCompare::Sync();
			}

			//2: check for breakpoint
			if (BreakPoints::IsAddressBreakPoint(PC))
			{
				LOG(GEKKO, "Hit Breakpoint - %08x", PC);
				EnableStepping(true);
				if (BreakPoints::IsTempBreakPoint(PC))
					BreakPoints::Remove(PC);
				break;
			}

			//3: do a step
			PowerPC::SingleStep();
			break;

		case CPU_STEPPING:
			//1: wait for step command..
			m_StepEvent.Wait();
			
			//2: check for cpu compare
			if (CPUCompare::IsEnabled() && g_Branch)
			{
				g_Branch = false;
				CPUCompare::Sync();
			}

			//3: do a step
			PowerPC::SingleStep();
			
			//4: update disasm dialog
			if (m_SyncEvent) {
				m_SyncEvent->Set();
				m_SyncEvent = 0;
			}
			Host_UpdateDisasmDialog();
			break;

		case CPU_POWERDOWN:
			//1: Exit loop!!
			return; 
	}
}

void CCPU::Break() 
{
	EnableStepping(true);
}
