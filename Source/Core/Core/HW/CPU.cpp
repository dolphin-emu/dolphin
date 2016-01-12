// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/VideoBackendBase.h"

namespace
{
	static Common::Event m_StepEvent;
	static Common::Event *m_SyncEvent = nullptr;
	static std::mutex m_csCpuOccupied;
}

namespace CPU
{

void Init(int cpu_core)
{
	PowerPC::Init(cpu_core);
	m_SyncEvent = nullptr;
}

void Shutdown()
{
	PowerPC::Shutdown();
	m_SyncEvent = nullptr;
}

void Run()
{
	std::lock_guard<std::mutex> lk(m_csCpuOccupied);
	Host_UpdateDisasmDialog();

	while (true)
	{
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
				continue;

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

void Stop()
{
	PowerPC::Stop();
	m_StepEvent.Set();
}

bool IsStepping()
{
	return PowerPC::GetState() == PowerPC::CPU_STEPPING;
}

void Reset()
{
}

void StepOpcode(Common::Event* event)
{
	m_StepEvent.Set();
	if (PowerPC::GetState() == PowerPC::CPU_STEPPING)
	{
		m_SyncEvent = event;
	}
}

void EnableStepping(const bool stepping)
{
	if (stepping)
	{
		PowerPC::Pause();
		m_StepEvent.Reset();
		EmulatorState(false);
		AudioCommon::ClearAudioBuffer(true);
	}
	else
	{
		// SingleStep so that the "continue", "step over" and "step out" debugger functions
		// work when the PC is at a breakpoint at the beginning of the block
		// If watchpoints are enabled, any instruction could be a breakpoint.
		bool could_be_bp;
#ifdef ENABLE_MEM_CHECK
		could_be_bp = true;
#else
		could_be_bp = PowerPC::breakpoints.IsAddressBreakPoint(PC);
#endif
		if (could_be_bp && PowerPC::GetMode() != PowerPC::MODE_INTERPRETER)
		{
			PowerPC::CoreMode oldMode = PowerPC::GetMode();
			PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
			PowerPC::SingleStep();
			PowerPC::SetMode(oldMode);
		}
		PowerPC::Start();
		m_StepEvent.Set();
		EmulatorState(true);
		AudioCommon::ClearAudioBuffer(false);
	}
}

void Break()
{
	EnableStepping(true);
}

bool PauseAndLock(bool do_lock, bool unpause_on_unlock)
{
	static bool s_have_fake_cpu_thread;
	bool wasUnpaused = !IsStepping();
	if (do_lock)
	{
		// we can't use EnableStepping, that would causes deadlocks with both audio and video
		PowerPC::Pause();
		if (!Core::IsCPUThread())
		{
			m_csCpuOccupied.lock();
			s_have_fake_cpu_thread = true;
			Core::DeclareAsCPUThread();
		}
		else
		{
			s_have_fake_cpu_thread = false;
		}
	}
	else
	{
		if (unpause_on_unlock)
		{
			PowerPC::Start();
			m_StepEvent.Set();
		}

		if (s_have_fake_cpu_thread)
		{
			Core::UndeclareAsCPUThread();
			m_csCpuOccupied.unlock();
			s_have_fake_cpu_thread = false;
		}
	}
	return wasUnpaused;
}

}
