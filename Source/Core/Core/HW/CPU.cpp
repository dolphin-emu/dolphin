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
	static Common::Event *m_SyncEvent = nullptr;
	static std::mutex m_csCpuOccupied;
}

static std::condition_variable s_cpu_step_cvar;
static bool s_cpu_step_instruction = false;

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
}

void Shutdown()
{
	Stop();
	PowerPC::Shutdown();
}

// Requires holding m_csCpuOccupied
static inline void FlushStepSyncEventLocked()
{
	if (m_SyncEvent)
	{
		m_SyncEvent->Set();
		m_SyncEvent = nullptr;
	}
	s_cpu_step_instruction = false;
}

void Run()
{
	std::unique_lock<std::mutex> lk(m_csCpuOccupied);
	while (true)
	{
		switch (PowerPC::GetState())
		{
		case PowerPC::CPU_RUNNING:
			// 1: Adjust PC for JIT when debugging
			// SingleStep so that the "continue", "step over" and "step out" debugger functions
			// work when the PC is at a breakpoint at the beginning of the block
			// If watchpoints are enabled, any instruction could be a breakpoint.
			if (PowerPC::GetMode() != PowerPC::MODE_INTERPRETER)
			{
#ifndef ENABLE_MEM_CHECK
				if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
#endif
				{
					PowerPC::CoreMode old_mode = PowerPC::GetMode();
					PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
					PowerPC::SingleStep();
					PowerPC::SetMode(old_mode);
				}
			}

			// 2: enter a fast runloop
			PowerPC::RunLoop();
			break;

		case PowerPC::CPU_STEPPING:
			// 1: wait for step command..
			s_cpu_step_cvar.wait(lk, [] { return s_cpu_step_instruction || PowerPC::GetState() != PowerPC::CPU_STEPPING; });

			// NOTE: Because GetState is unsynchronized there may be transient failures
			//   i.e. GetState is still CPU_STEPPING and s_cpu_step_instruction is still false
			if (!s_cpu_step_instruction || PowerPC::GetState() != PowerPC::CPU_STEPPING)
			{
				// If the mode is changed, signal event immediately
				FlushStepSyncEventLocked();
				continue;
			}

			// 2: do a step
			PowerPC::SingleStep();

			// 3: update disasm dialog
			FlushStepSyncEventLocked();
			Host_UpdateDisasmDialog();
			break;

		case PowerPC::CPU_POWERDOWN:
			// 1: Exit loop!!
			Host_UpdateDisasmDialog();
			return;
		}
	}
}

void Stop()
{
	PowerPC::Stop(true);
	std::lock_guard<std::mutex> guard(m_csCpuOccupied);
	FlushStepSyncEventLocked();
	s_cpu_step_cvar.notify_one();
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
	if (PowerPC::GetState() != PowerPC::CPU_STEPPING)
	{
		if (event)
			event->Set();
		return;
	}
	std::lock_guard<std::mutex> guard(m_csCpuOccupied);
	s_cpu_step_instruction = true;
	// Potential race where the previous step has not been serviced yet.
	if (m_SyncEvent && m_SyncEvent != event)
		m_SyncEvent->Set();
	m_SyncEvent = event;
	s_cpu_step_cvar.notify_one();
}

void StealStepEventAndWaitOnIt()
{
	std::unique_lock<std::mutex> guard(m_csCpuOccupied);
	s_cpu_step_cvar.wait(guard, [] { return PowerPC::GetState() != PowerPC::CPU_STEPPING || s_cpu_step_instruction; });
	FlushStepSyncEventLocked();
}

static void SetSteppingEnabled(bool synchronize)
{
	PowerPC::Pause(synchronize);
	Fifo::EmulatorState(false);
	AudioCommon::ClearAudioBuffer(true);
}

void HostEnterEnableSteppingLock()
{
	std::unique_lock<std::mutex> guard(s_stepping_lock);
	s_stepping_host_control += 1;
	// Wait for CPU::Break to finish
	s_stepping_cvar.wait(guard, []{ return !s_stepping_system_control; });
}

void HostLeaveEnableSteppingLock(bool did_enable_step)
{
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
	std::lock_guard<std::mutex> guard(s_stepping_lock);
	return !s_stepping_system_leave_stepping;
}

void EnableStepping(bool stepping)
{
	if (stepping)
	{
		HostEnterEnableSteppingLock();
		SetSteppingEnabled(true);
		HostLeaveEnableSteppingLock(true);
	}
	else
	{
		// Wait for the CPU state to settle into its stop state.
		// We need m_csCpuOccupied to use s_cpu_step_cvar.
		PauseAndLock(true, false);

		Fifo::EmulatorState(true);
		AudioCommon::ClearAudioBuffer(false);

		// We are going to unpause on unlock
		PauseAndLock(false, true);
	}
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
	static bool s_have_fake_cpu_thread = false;
	bool was_unpaused = !IsStepping();
	if (do_lock)
	{
		HostEnterEnableSteppingLock();
		// we can't use EnableStepping, that would cause deadlocks with both audio and video
		PowerPC::Pause(true);
		m_csCpuOccupied.lock();
		if (!Core::IsCPUThread())
		{
			s_have_fake_cpu_thread = true;
			Core::DeclareAsCPUThread();
		}
	}
	else
	{
		if (unpause_on_unlock)
		{
			PowerPC::Start();
			s_cpu_step_cvar.notify_one();
		}
		if (s_have_fake_cpu_thread)
		{
			Core::UndeclareAsCPUThread();
			s_have_fake_cpu_thread = false;
		}
		m_csCpuOccupied.unlock();
		HostLeaveEnableSteppingLock(!unpause_on_unlock);
	}
	return was_unpaused;
}

}
