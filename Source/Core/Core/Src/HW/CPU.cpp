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
#include "Thread.h"

#include "../DSPEmulator.h"
#include "../PowerPC/PowerPC.h"
#include "../Host.h"
#include "../Core.h"
#include "CPU.h"
#include "DSP.h"
#include "Movie.h"

#include "VideoBackendBase.h"

namespace
{
	static Common::Event m_StepEvent;
	static Common::Event *m_SyncEvent;
	static std::mutex m_csCpuOccupied;
}

void CCPU::Init(int cpu_core)
{
	if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
	{
		cpu_core = Movie::GetCPUMode();
	}
	PowerPC::Init(cpu_core);
	m_SyncEvent = 0;
}

void CCPU::Shutdown()
{
	PowerPC::Shutdown();
	m_SyncEvent = 0;
}

void CCPU::Run()
{
	std::lock_guard<std::mutex> lk(m_csCpuOccupied);
	Host_UpdateDisasmDialog();

	while (true)
	{
reswitch:
		switch (PowerPC::GetState())
		{
		case PowerPC::CPU_RUNNING:
			//1: enter a fast runloop
			PowerPC::RunLoop();
			break;

		case PowerPC::CPU_STEPPING:
			m_csCpuOccupied.unlock();

			//1: wait for step command..
			m_StepEvent.Wait();

			m_csCpuOccupied.lock();
			if (PowerPC::GetState() == PowerPC::CPU_POWERDOWN)
				return;
			if (PowerPC::GetState() != PowerPC::CPU_STEPPING)
				goto reswitch;

			//3: do a step
			PowerPC::SingleStep();

			//4: update disasm dialog
			if (m_SyncEvent) {
				m_SyncEvent->Set();
				m_SyncEvent = 0;
			}
			Host_UpdateDisasmDialog();
			break;

		case PowerPC::CPU_POWERDOWN:
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
	return PowerPC::GetState() == PowerPC::CPU_STEPPING;
}

void CCPU::Reset()
{

} 

void CCPU::StepOpcode(Common::Event *event) 
{
	m_StepEvent.Set();
	if (PowerPC::GetState() == PowerPC::CPU_STEPPING)
	{
		m_SyncEvent = event;
	}
}

void CCPU::EnableStepping(const bool _bStepping)
{	
	if (_bStepping)
	{
		PowerPC::Pause();
		m_StepEvent.Reset();
		g_video_backend->EmuStateChange(EMUSTATE_CHANGE_PAUSE);
		DSP::GetDSPEmulator()->DSP_ClearAudioBuffer(true);
	}
	else
	{
		PowerPC::Start();
		m_StepEvent.Set();
		g_video_backend->EmuStateChange(EMUSTATE_CHANGE_PLAY);
		DSP::GetDSPEmulator()->DSP_ClearAudioBuffer(false);
	}
}

void CCPU::Break() 
{
	EnableStepping(true);
}

bool CCPU::PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	bool wasUnpaused = !IsStepping();
	if (doLock)
	{
		// we can't use EnableStepping, that would causes deadlocks with both audio and video
		PowerPC::Pause();
		if (!Core::IsCPUThread())
			m_csCpuOccupied.lock();
	}
	else
	{
		if (unpauseOnUnlock)
		{
			PowerPC::Start();
			m_StepEvent.Set();
		}
		if (!Core::IsCPUThread())
			m_csCpuOccupied.unlock();
	}
	return wasUnpaused;
}
