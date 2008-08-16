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
#include "../PatchEngine.h"
#include "SystemTimers.h"
#include "../Plugins/Plugin_DSP.h"
#include "../Plugins/Plugin_Video.h"
#include "../HW/DSP.h"
#include "../HW/AudioInterface.h"
#include "../HW/VideoInterface.h"
#include "../HW/CommandProcessor.h"
#include "../HW/SerialInterface.h"
#include "../PowerPC/PowerPC.h"
#include "../CoreTiming.h"
#include "../Core.h"
#include "../IPC_HLE/WII_IPC_HLE.h"
#include "Thread.h"
#include "Timer.h"

namespace SystemTimers
{

u32 CPU_CORE_CLOCK  = 486000000u;             // 486 mhz (its not 485, stop bugging me!)

const int ThrottleFrequency = 60;
s64 fakeDec;

//ratio of TB and Decrementer to clock cycles
//4 or 8? not really sure, but think it's 8
enum {
	TIMER_RATIO = 8
};

// These are badly educated guesses
// Feel free to experiment
int
	// update VI often to let it go through its scanlines with decent accuracy
	// Maybe should actually align this with the scanline update? Current method in 
	// VideoInterface::Update is stupid!
	VI_PERIOD = GetTicksPerSecond() / (60*120), 

	// TODO: The SI interfact actually has a register that determines the polling frequency.
	// We should obey that instead of arbitrarly checking at 60fps.
	SI_PERIOD = GetTicksPerSecond() / 60, //once a frame is good for controllers
	
	// These are the big question marks IMHO :)
	// This one should simply be determined by the increasing counter in AI.
	AI_PERIOD = GetTicksPerSecond() / 80,

	// These shouldn't be period controlled either, most likely.
	DSP_PERIOD = GetTicksPerSecond() / 250,

	// This is completely arbitrary. If we find that we need lower latency, we can just
	// increase this number.
    HLE_IPC_PERIOD = GetTicksPerSecond() / 250,

	// This one is also fairly arbitrary. Every N cycles, run the GPU until it starves (in single core mode only).
	GPU_PERIOD = 10000;

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
	CoreTiming::ScheduleEvent(AI_PERIOD-cyclesLate, &AICallback, "AICallback");
}

void DSPCallback(u64 userdata, int cyclesLate)
{
	// ~1/6th as many cycles as the period PPC-side.
	PluginDSP::DSP_Update(DSP_PERIOD / 6);
	CoreTiming::ScheduleEvent(DSP_PERIOD-cyclesLate, &DSPCallback, "DSPCallback");
}

void AudioFifoCallback(u64 userdata, int cyclesLate)
{	
	int period = CPU_CORE_CLOCK / (AudioInterface::GetDSPSampleRate() * 4 / 32);
	DSP::UpdateAudioDMA();  // Push audio to speakers.

	CoreTiming::ScheduleEvent(period - cyclesLate, &AudioFifoCallback, "AudioFifoCallback");
}

void IPC_HLE_UpdateCallback(u64 userdata, int cyclesLate)
{	
    WII_IPC_HLE_Interface::Update();
    CoreTiming::ScheduleEvent(HLE_IPC_PERIOD-cyclesLate, &IPC_HLE_UpdateCallback, "IPC_HLE_UpdateCallback");
}

void VICallback(u64 userdata, int cyclesLate)
{
	VideoInterface::Update();
	CoreTiming::ScheduleEvent(VI_PERIOD-cyclesLate, &VICallback, "VICallback");
}

void SICallback(u64 userdata, int cyclesLate)
{
	// This is once per frame - good candidate for patching stuff
	PatchEngine_ApplyFramePatches();
	// OK, do what we are here to do.
	SerialInterface::UpdateDevices();
	CoreTiming::ScheduleEvent(SI_PERIOD-cyclesLate, &SICallback, "SICallback");
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
	fakeDec = decValue*TIMER_RATIO;
	CoreTiming::RemoveEvent(DecrementerCallback);
	CoreTiming::ScheduleEvent(decValue * TIMER_RATIO, DecrementerCallback, "DecCallback");
}

void AdvanceCallback(int cyclesExecuted)
{
	fakeDec -= cyclesExecuted;
	u64 timebase_ticks = CoreTiming::GetTicks() / TIMER_RATIO; //works since we are little endian and TL comes first :)
	*(u64*)&TL = timebase_ticks;
 	if (fakeDec >= 0)
		PowerPC::ppcState.spr[SPR_DEC] = (u32)fakeDec / TIMER_RATIO;
}

void RunGPUCallback(u64 userdata, int cyclesLate)
{
	CommandProcessor::CatchUpGPU();
	CoreTiming::ScheduleEvent(GPU_PERIOD-cyclesLate, &RunGPUCallback, "RunGPUCallback");
}

// TODO(ector): improve, by using a more accurate timer
// calculate the timing over the past 7 frames
// calculating over all time doesn't work : if it's slow for a while, will run like crazy after that
// calculating over just 1 frame is too shaky
#define HISTORYLENGTH 7
int timeHistory[HISTORYLENGTH] = {0,0,0,0,0};

void Throttle(u64 userdata, int cyclesLate)
{
	if (!Core::GetStartupParameter().bThrottle)
		return;
	static Common::Timer timer;

	for (int i=0; i<HISTORYLENGTH-1; i++)
		timeHistory[i] = timeHistory[i+1];

	int t = (int)timer.GetTimeDifference();
	//timer.Update();

	if (timeHistory[0] != 0)
	{
		const int delta = (int)(1000*(HISTORYLENGTH-1)/ThrottleFrequency);
		while (t - timeHistory[0] < delta)
		{
			// ugh, busy wait
			Common::SleepCurrentThread(0);
			t = (int)timer.GetTimeDifference();
		}
	}
	timeHistory[HISTORYLENGTH-1] = t;
	CoreTiming::ScheduleEvent((int)(GetTicksPerSecond()/ThrottleFrequency)-cyclesLate, &Throttle, "Throttle");
}

void Init()
{
	if (Core::GetStartupParameter().bWii)
	{
		CPU_CORE_CLOCK = 721000000;
		VI_PERIOD = GetTicksPerSecond() / (60*120); 
		SI_PERIOD = GetTicksPerSecond() / 60; // once a frame is good for controllers
		
		// These are the big question marks IMHO :)
		AI_PERIOD = GetTicksPerSecond() / 80;
		DSP_PERIOD = (int)(GetTicksPerSecond() * 0.003f);

        HLE_IPC_PERIOD = (int)(GetTicksPerSecond() * 0.003f);
	}
	else
	{
		CPU_CORE_CLOCK = 486000000;
		VI_PERIOD = GetTicksPerSecond() / (60*120);
		SI_PERIOD = GetTicksPerSecond() / 60; // once a frame is good for controllers
		
		// These are the big question marks IMHO :)
		AI_PERIOD = GetTicksPerSecond() / 80;
		DSP_PERIOD = (int)(GetTicksPerSecond() * 0.005f);
	}
	Common::Timer::IncreaseResolution();
	memset(timeHistory, 0, sizeof(timeHistory));
	CoreTiming::Clear();

	CoreTiming::ScheduleEvent((int)(GetTicksPerSecond() / ThrottleFrequency), &Throttle, "Throttle");

	CoreTiming::ScheduleEvent(AI_PERIOD, &AICallback, "AICallback");
	CoreTiming::ScheduleEvent(VI_PERIOD, &VICallback, "VICallback");
	CoreTiming::ScheduleEvent(DSP_PERIOD, &DSPCallback, "DSPCallback");
	CoreTiming::ScheduleEvent(SI_PERIOD, &SICallback, "SICallback");
	CoreTiming::ScheduleEvent(CPU_CORE_CLOCK / (32000 * 4 / 32), &AudioFifoCallback, "AudioFifoCallback");

	if (!Core::GetStartupParameter().bUseDualCore)
		CoreTiming::ScheduleEvent(GPU_PERIOD, &RunGPUCallback, "RunGPUCallback");

    if (Core::GetStartupParameter().bWii)
    {
        CoreTiming::ScheduleEvent(HLE_IPC_PERIOD, &IPC_HLE_UpdateCallback, "IPC_HLE_UpdateCallback");
    }

	CoreTiming::RegisterAdvanceCallback(&AdvanceCallback);
}

void Shutdown()
{
	Common::Timer::RestoreResolution();
	CoreTiming::Clear();
}

}
