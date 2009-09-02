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



// File description: This file controls all system timers
/* -------------
   "Time" is measured in frames, not time: These update frequencies are determined by the passage
   of frames. So if a game runs slow, on a slow computer for example, these updates will occur
   less frequently. This makes sense because almost all console games are controlled by frames
   rather than time, so if a game can't keep up with the normal framerate all animations and
   actions slows down and the game runs to slow. This is different from PC games that are are
   often controlled by time instead and may not have maximum framerates.

   However, I'm not sure if the Bluetooth communication for the Wiimote is entirely frame
   dependent, the timing problems with the ack command in Zelda - TP may be related to
   time rather than frames? For now the IPC_HLE_PERIOD is frame dependet, but bcause of
   different conditions on the way to PluginWiimote::Wiimote_Update() the updates may actually
   be time related after all, or not?

   I'm not sure about this but the text below seems to assume that 60 fps means that the game
   runs in the normal intended speed. In that case an update time of [GetTicksPerSecond() / 60]
   would mean one update per frame and [GetTicksPerSecond() / 250] would mean four updates per
   frame.
   
   
   IPC_HLE_PERIOD: For the Wiimote this is the call scedule:
		IPC_HLE_UpdateCallback() // In this file

			// This function seems to call all devices' Update() function four times per frame
			WII_IPC_HLE_Interface::Update()

				// If the AclFrameQue is empty this will call Wiimote_Update() and make it send
				the current input status to the game. I'm not sure if this occurs approximately
				once every frame or if the frequency is not exactly tied to rendered frames
				CWII_IPC_HLE_Device_usb_oh1_57e_305::Update()
					PluginWiimote::Wiimote_Update()

				// This is also a device updated by WII_IPC_HLE_Interface::Update() but it doesn't
				seem to ultimately call PluginWiimote::Wiimote_Update(). However it can be called
				by the /dev/usb/oh1 device if the AclFrameQue is empty.
				CWII_IPC_HLE_WiiMote::Update()
//////////////////////////*/



// Inlude
// -------------
#include "Common.h"
#include "../PatchEngine.h"
#include "SystemTimers.h"
#include "../PluginManager.h"
#include "../HW/DSP.h"
#include "../HW/AudioInterface.h"
#include "../HW/VideoInterface.h"
#include "../HW/SI.h"
#include "../HW/CommandProcessor.h" // for DC watchdog hack
#include "../HW/EXI_DeviceIPL.h"
#include "../PowerPC/PowerPC.h"
#include "../CoreTiming.h"
#include "../Core.h"
#include "../IPC_HLE/WII_IPC_HLE.h"
#include "Thread.h"
#include "Timer.h"



namespace SystemTimers
{


// Declarations and definitions
// -------------
u32 CPU_CORE_CLOCK  = 486000000u;             // 486 mhz (its not 485, stop bugging me!)

s64 fakeDec;
u64 startTimeBaseTicks;

// Ratio of TB and Decrementer to clock cycles.
// TB clk is 1/4 of BUS clk. And it seems BUS clk is really 1/3 of CPU clk.
// So, ratio is 1 / (1/4 * 1/3 = 1/12) = 12.
// note: ZWW is ok and faster with TIMER_RATIO=8 though.
// !!! POSSIBLE STABLE PERF BOOST HACK THERE !!!
enum
{
	TIMER_RATIO = 12
};

int et_Dec;
int et_VI;
int et_SI;
int et_AI;
int et_AudioFifo;
int et_DSP;
int et_IPC_HLE;
int et_FakeGPWD; // for DC watchdog hack

// These are badly educated guesses
// Feel free to experiment. Set these in Init below.
int
	// TODO: The SI in fact actually has a register that determines the polling frequency.
	// We should obey that instead of arbitrarly checking at 60fps.
	SI_PERIOD, // once a frame is good for controllers

	// This one should simply be determined by the increasing counter in AI.
	AI_PERIOD,

	// These shouldn't be period controlled either, most likely.
	DSP_PERIOD,

	// This is completely arbitrary. If we find that we need lower latency, we can just
	// increase this number.
    IPC_HLE_PERIOD,

	// For DC watchdog hack
	// Once every 4 frame-period seems to be enough (arbitrary taking 60fps as the ref).
	// TODO: make it VI output frame rate compliant (30/60 and 25/50)
	// Assuming game's frame-finish-watchdog wait more than 4 emulated frame-period before starting its mess.
	FAKE_GP_WATCHDOG_PERIOD;



u32 GetTicksPerSecond()
{
	return CPU_CORE_CLOCK;
}

u32 ConvertMillisecondsToTicks(u32 _Milliseconds)
{
	return GetTicksPerSecond() / 1000 * _Milliseconds;
}

void AICallback(u64 userdata, int cyclesLate)
{
	// Update disk streaming. All that code really needs a revamp, including replacing the codec with the one
	// from in_cube.
	AudioInterface::Update();
	CoreTiming::ScheduleEvent(AI_PERIOD - cyclesLate, et_AI);
}

// DSP/CPU timeslicing.
void DSPCallback(u64 userdata, int cyclesLate)
{
	// ~1/6th as many cycles as the period PPC-side.
    CPluginManager::GetInstance().GetDSP()->DSP_Update(DSP_PERIOD / 6);
	CoreTiming::ScheduleEvent(DSP_PERIOD - cyclesLate, et_DSP);
}

void AudioFifoCallback(u64 userdata, int cyclesLate)
{
	int period = CPU_CORE_CLOCK / (AudioInterface::GetDSPSampleRate() * 4 / 32);
	DSP::UpdateAudioDMA();  // Push audio to speakers.

	CoreTiming::ScheduleEvent(period - cyclesLate, et_AudioFifo);
}

void IPC_HLE_UpdateCallback(u64 userdata, int cyclesLate)
{
    WII_IPC_HLE_Interface::UpdateDevices();
    CoreTiming::ScheduleEvent(IPC_HLE_PERIOD-cyclesLate, et_IPC_HLE);
}

void VICallback(u64 userdata, int cyclesLate)
{
	if (Core::GetStartupParameter().bWii)
		WII_IPC_HLE_Interface::Update();

	VideoInterface::Update();
	CoreTiming::ScheduleEvent(VideoInterface::getTicksPerLine() - cyclesLate, et_VI);
}

void SICallback(u64 userdata, int cyclesLate)
{
	SerialInterface::UpdateDevices();
	CoreTiming::ScheduleEvent(SI_PERIOD-cyclesLate, et_SI);

	// This is once per frame - good candidate for patching stuff
	// Patch mem and run the Action Replay
	PatchEngine::ApplyFramePatches();
	PatchEngine::ApplyARPatches();
}

void DecrementerCallback(u64 userdata, int cyclesLate)
{
	//Why is fakeDec too big here?
	fakeDec = -1;
	PowerPC::ppcState.spr[SPR_DEC] = 0xFFFFFFFF;
	PowerPC::ppcState.Exceptions |= EXCEPTION_DECREMENTER;
}

void DecrementerSet()
{
	u32 decValue = PowerPC::ppcState.spr[SPR_DEC];
	fakeDec = decValue * TIMER_RATIO;
	CoreTiming::RemoveEvent(et_Dec);
	CoreTiming::ScheduleEvent(decValue * TIMER_RATIO, et_Dec);
}

void AdvanceCallback(int cyclesExecuted)
{
    if (PowerPC::GetState() != PowerPC::CPU_RUNNING)
        return;

	fakeDec -= cyclesExecuted;
	u64 timebase_ticks = CoreTiming::GetTicks() / TIMER_RATIO;
	*(u64*)&TL = timebase_ticks + startTimeBaseTicks;  //works since we are little endian and TL comes first :)
 	if (fakeDec >= 0)
		PowerPC::ppcState.spr[SPR_DEC] = (u32)fakeDec / TIMER_RATIO;
}


// For DC watchdog hack
void FakeGPWatchdogCallback(u64 userdata, int cyclesLate)
{
	CommandProcessor::WaitForFrameFinish(); // lock CPUThread until frame finish
	CoreTiming::ScheduleEvent(FAKE_GP_WATCHDOG_PERIOD-cyclesLate, et_FakeGPWD);
}

void Init()
{
	FAKE_GP_WATCHDOG_PERIOD = GetTicksPerSecond() / 60;
	if (Core::GetStartupParameter().bWii)
	{
		CPU_CORE_CLOCK = 729000000u;
		SI_PERIOD = GetTicksPerSecond() / 60; // once a frame is good for controllers

		if (!Core::GetStartupParameter().bDSPThread) {
			DSP_PERIOD = 12000;  // TO BE TWEAKED
		} else {
			DSP_PERIOD = (int)(GetTicksPerSecond() * 0.003f);
		}

		// This is the biggest question mark.
		AI_PERIOD = GetTicksPerSecond() / 80;

        IPC_HLE_PERIOD = (int)(GetTicksPerSecond() * 0.003f);
	}
	else
	{
		CPU_CORE_CLOCK = 486000000;
		SI_PERIOD = GetTicksPerSecond() / 60; // once a frame is good for controllers
	
		if (!Core::GetStartupParameter().bDSPThread) {
			DSP_PERIOD = 12000;  // TO BE TWEAKED
		} else {
			DSP_PERIOD = (int)(GetTicksPerSecond() * 0.005f);
		}

		// This is the biggest question mark.
		AI_PERIOD = GetTicksPerSecond() / 80;
	}
	Common::Timer::IncreaseResolution();
	// store and convert localtime at boot to timebase ticks
	startTimeBaseTicks = (u64)(CPU_CORE_CLOCK / TIMER_RATIO) * (u64)CEXIIPL::GetGCTime();

	et_Dec = CoreTiming::RegisterEvent("DecCallback", DecrementerCallback);
	et_AI = CoreTiming::RegisterEvent("AICallback", AICallback);
	et_VI = CoreTiming::RegisterEvent("VICallback", VICallback);
	et_SI = CoreTiming::RegisterEvent("SICallback", SICallback);
	et_DSP = CoreTiming::RegisterEvent("DSPCallback", DSPCallback);
	et_AudioFifo = CoreTiming::RegisterEvent("AudioFifoCallback", AudioFifoCallback);
	et_IPC_HLE = CoreTiming::RegisterEvent("IPC_HLE_UpdateCallback", IPC_HLE_UpdateCallback);
	// Always register this. Increases chances of DC/SC save state compatibility.
	et_FakeGPWD = CoreTiming::RegisterEvent("FakeGPWatchdogCallback", FakeGPWatchdogCallback);

	CoreTiming::ScheduleEvent(AI_PERIOD, et_AI);
	CoreTiming::ScheduleEvent(VideoInterface::getTicksPerLine(), et_VI);
	CoreTiming::ScheduleEvent(DSP_PERIOD, et_DSP);
	CoreTiming::ScheduleEvent(SI_PERIOD, et_SI);
	CoreTiming::ScheduleEvent(CPU_CORE_CLOCK / (32000 * 4 / 32), et_AudioFifo);

	// For DC watchdog hack
	if (Core::GetStartupParameter().bUseDualCore)
	{
		CoreTiming::ScheduleEvent(FAKE_GP_WATCHDOG_PERIOD, et_FakeGPWD);
	}

    if (Core::GetStartupParameter().bWii)
        CoreTiming::ScheduleEvent(IPC_HLE_PERIOD, et_IPC_HLE);
  
	CoreTiming::RegisterAdvanceCallback(&AdvanceCallback);
}

void Shutdown()
{
	Common::Timer::RestoreResolution();
}

}  // namespace
