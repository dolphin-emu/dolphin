// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <condition_variable>
#include <mutex>

#include "AudioCommon/AudioCommon.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "VideoCommon/Fifo.h"

namespace
{
	static Common::Event m_StepEvent;
	static Common::Event *m_SyncEvent = nullptr;
	static std::mutex m_csCpuOccupied;
}

static std::mutex s_stepping_lock;
static std::condition_variable s_stepping_cvar;
static unsigned int s_stepping_host_control = 0;
static bool s_stepping_system_leave_stepping = false;
static bool s_stepping_system_control = false;

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
	std::unique_lock<std::mutex> lk(m_csCpuOccupied);
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
			lk.unlock();

			//1: wait for step command..
			m_StepEvent.Wait();

			lk.lock();
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
	PowerPC::Stop(true);
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

void StealStepEventAndWaitOnIt()
{
	m_StepEvent.Wait();
	if (m_SyncEvent)
	{
		m_SyncEvent->Set();
		m_SyncEvent = nullptr;
	}
}

static void SetSteppingEnabled(bool synchronize)
{
	PowerPC::Pause(synchronize);
	m_StepEvent.Reset();
	Fifo::EmulatorState(false);
	AudioCommon::ClearAudioBuffer(true);
}

void HostEnterEnableSteppingLock()
{
	//_assert_(Core::IsHostThread());
	std::unique_lock<std::mutex> guard(s_stepping_lock);
	s_stepping_host_control += 1;
	// Wait for CPU::Break to finish
	s_stepping_cvar.wait(guard, []{ return !s_stepping_system_control; });
}

void HostLeaveEnableSteppingLock(bool did_enable_step)
{
	//_assert_(Core::IsHostThread());
	std::unique_lock<std::mutex> guard(s_stepping_lock);
	if (s_stepping_system_leave_stepping && s_stepping_host_control == 1)
	{
		if (!did_enable_step)
		{
			guard.unlock();
			SetSteppingEnabled(true);
			guard.lock();
		}
		s_stepping_system_leave_stepping = false;
	}
	s_stepping_host_control -= 1;
}

bool HostAllowUnpauseWhenLeavingPauseAndLock()
{
	//_assert_(Core::IsHostThread());
	std::lock_guard<std::mutex> guard(s_stepping_lock);
	return !s_stepping_system_leave_stepping;
}

void EnableStepping(bool stepping)
{
	HostEnterEnableSteppingLock();
	if (stepping)
	{
		SetSteppingEnabled(true);
	}
	else
	{
		// Wait for PowerPC::RunLoop to actually exit before we try to access the
		// register state or step the CPU.
		PowerPC::Pause(true);
		std::unique_lock<std::mutex> guard(m_csCpuOccupied);

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
			// FIXME: Why isn't this inside CPU::Run()? We can do this in
			//	"case PowerPC::CPU_RUNNING" before the call to PowerPC::RunLoop()
			PowerPC::CoreMode oldMode = PowerPC::GetMode();
			PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
			// CoreTiming will Advance() and trigger a SetInterrupt
			// from the wrong thread.
			bool not_cpu = !Core::IsCPUThread();
			if (not_cpu)
				Core::DeclareAsCPUThread();
			PowerPC::SingleStep();
			if (not_cpu)
				Core::UndeclareAsCPUThread();
			PowerPC::SetMode(oldMode);
		}
		guard.unlock();
		PowerPC::Start();
		m_StepEvent.Set();
		Fifo::EmulatorState(true);
		AudioCommon::ClearAudioBuffer(false);
	}
	HostLeaveEnableSteppingLock(stepping);
}

void Break()
{
	std::unique_lock<std::mutex> guard(s_stepping_lock);

	// If another thread (or ourself) is already trying to pause then let it do the work.
	if (s_stepping_system_control)
		return;
	// If the Host Thread is trying to EnableStepping(...) or PauseAndLock then we'll
	// deadlock, so just give up.
	if (s_stepping_host_control)
	{
		s_stepping_system_leave_stepping = true;
		return;
	}

	s_stepping_system_control = true;
	guard.unlock();
	// We'll deadlock if we synchronize, the CPU may block waiting for our caller to
	// finish resulting in the CPU loop never terminating.
	SetSteppingEnabled(false);
	guard.lock();
	s_stepping_system_control = false;
	// Wake EnableStepping or Core::PauseAndLock
	s_stepping_cvar.notify_one();
}

bool PauseAndLock(bool do_lock, bool unpause_on_unlock)
{
	static bool s_have_fake_cpu_thread;
	bool was_unpaused = !IsStepping();
	if (do_lock)
	{
		// we can't use EnableStepping, that would cause deadlocks with both audio and video
		PowerPC::Pause(true);
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
	return was_unpaused;
}

}
