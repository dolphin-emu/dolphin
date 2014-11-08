// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <mutex>

#include "AudioCommon/AudioCommon.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Core/Core.h"
#include "Core/DSPEmulator.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/PowerPC/PowerPC.h"
#include "VideoCommon/VideoBackendBase.h"

namespace
{
	static Common::Event m_StepEvent;
	static Common::Event *m_SyncEvent = nullptr;
	static std::mutex m_csCpuOccupied;
}

void CCPU::Init(int cpu_core)
{
	PowerPC::Init(cpu_core);
	m_SyncEvent = nullptr;
}

void CCPU::Shutdown()
{
	PowerPC::Shutdown();
	m_SyncEvent = nullptr;
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
			if (m_SyncEvent)
			{
				m_SyncEvent->Set();
				m_SyncEvent = nullptr;
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
		AudioCommon::ClearAudioBuffer(true);
	}
	else
	{
		// SingleStep so that the "continue", "step over" and "step out" debugger functions
		// work when the PC is at a breakpoint at the beginning of the block
		if (PowerPC::breakpoints.IsAddressBreakPoint(PC) && PowerPC::GetMode() != PowerPC::MODE_INTERPRETER)
		{
			PowerPC::CoreMode oldMode = PowerPC::GetMode();
			PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
			PowerPC::SingleStep();
			PowerPC::SetMode(oldMode);
		}
		PowerPC::Start();
		m_StepEvent.Set();
		g_video_backend->EmuStateChange(EMUSTATE_CHANGE_PLAY);
		AudioCommon::ClearAudioBuffer(false);
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
