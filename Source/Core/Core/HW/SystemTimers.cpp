// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// This file controls all system timers

/* (shuffle2) I don't know who wrote this, but take it with salt. For starters, "time" is contextual...
"Time" is measured in frames, not time: These update frequencies are determined by the passage
of frames. So if a game runs slow, on a slow computer for example, these updates will occur
less frequently. This makes sense because almost all console games are controlled by frames
rather than time, so if a game can't keep up with the normal framerate all animations and
actions slows down and the game runs to slow. This is different from PC games that are
often controlled by time instead and may not have maximum framerates.

However, I'm not sure if the Bluetooth communication for the Wiimote is entirely frame
dependent, the timing problems with the ack command in Zelda - TP may be related to
time rather than frames? For now the IPC_HLE_PERIOD is frame dependent, but because of
different conditions on the way to PluginWiimote::Wiimote_Update() the updates may actually
be time related after all, or not?

I'm not sure about this but the text below seems to assume that 60 fps means that the game
runs in the normal intended speed. In that case an update time of [GetTicksPerSecond() / 60]
would mean one update per frame and [GetTicksPerSecond() / 250] would mean four updates per
frame.


IPC_HLE_PERIOD: For the Wiimote this is the call schedule:
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
*/

#include "Common/Atomic.h"
#include "Common/CommonTypes.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"
#include "Core/PatchEngine.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/DSP.h"
#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/SI.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/VideoBackendBase.h"


namespace SystemTimers
{

static u32 CPU_CORE_CLOCK  = 486000000u;             // 486 mhz (its not 485, stop bugging me!)

static int et_Dec;
static int et_VI;
static int et_SI;
static int et_CP;
static int et_AudioDMA;
static int et_DSP;
static int et_IPC_HLE;
static int et_PatchEngine; // PatchEngine updates every 1/60th of a second by default
static int et_Throttle;

// These are badly educated guesses
// Feel free to experiment. Set these in Init below.
static int
	// This is a fixed value, don't change it
	AUDIO_DMA_PERIOD,

	// Regulates the speed of the Command Processor
	CP_PERIOD,

	// This is completely arbitrary. If we find that we need lower latency, we can just
	// increase this number.
	IPC_HLE_PERIOD;



u32 GetTicksPerSecond()
{
	return CPU_CORE_CLOCK;
}

// DSP/CPU timeslicing.
static void DSPCallback(u64 userdata, int cyclesLate)
{
	//splits up the cycle budget in case lle is used
	//for hle, just gives all of the slice to hle
	DSP::UpdateDSPSlice(DSP::GetDSPEmulator()->DSP_UpdateRate() - cyclesLate);
	CoreTiming::ScheduleEvent(DSP::GetDSPEmulator()->DSP_UpdateRate() - cyclesLate, et_DSP);
}

static void AudioDMACallback(u64 userdata, int cyclesLate)
{
	int fields = VideoInterface::GetNumFields();
	int period = CPU_CORE_CLOCK / (AudioInterface::GetAIDSampleRate() * 4 / 32 * fields);
	DSP::UpdateAudioDMA();  // Push audio to speakers.
	CoreTiming::ScheduleEvent(period - cyclesLate, et_AudioDMA);
}

static void IPC_HLE_UpdateCallback(u64 userdata, int cyclesLate)
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		WII_IPC_HLE_Interface::UpdateDevices();
		CoreTiming::ScheduleEvent(IPC_HLE_PERIOD - cyclesLate, et_IPC_HLE);
	}
}

static void VICallback(u64 userdata, int cyclesLate)
{
	VideoInterface::Update();
	CoreTiming::ScheduleEvent(VideoInterface::GetTicksPerLine() - cyclesLate, et_VI);
}

static void SICallback(u64 userdata, int cyclesLate)
{
	SerialInterface::UpdateDevices();
	CoreTiming::ScheduleEvent(SerialInterface::GetTicksToNextSIPoll() - cyclesLate, et_SI);
}

static void CPCallback(u64 userdata, int cyclesLate)
{
	CommandProcessor::Update();
	CoreTiming::ScheduleEvent(CP_PERIOD - cyclesLate, et_CP);
}

static void DecrementerCallback(u64 userdata, int cyclesLate)
{
	PowerPC::ppcState.spr[SPR_DEC] = 0xFFFFFFFF;
	Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_DECREMENTER);
}

void DecrementerSet()
{
	u32 decValue = PowerPC::ppcState.spr[SPR_DEC];

	CoreTiming::RemoveEvent(et_Dec);
	if ((decValue & 0x80000000) == 0)
	{
		CoreTiming::SetFakeDecStartTicks(CoreTiming::GetTicks());
		CoreTiming::SetFakeDecStartValue(decValue);

		CoreTiming::ScheduleEvent(decValue * TIMER_RATIO, et_Dec);
	}
}

u32 GetFakeDecrementer()
{
	return (CoreTiming::GetFakeDecStartValue() - (u32)((CoreTiming::GetTicks() - CoreTiming::GetFakeDecStartTicks()) / TIMER_RATIO));
}

void TimeBaseSet()
{
	CoreTiming::SetFakeTBStartTicks(CoreTiming::GetTicks());
	CoreTiming::SetFakeTBStartValue(*((u64 *)&TL));
}

u64 GetFakeTimeBase()
{
	return CoreTiming::GetFakeTBStartValue() + ((CoreTiming::GetTicks() - CoreTiming::GetFakeTBStartTicks()) / TIMER_RATIO);
}

static void PatchEngineCallback(u64 userdata, int cyclesLate)
{
	// Patch mem and run the Action Replay
	PatchEngine::ApplyFramePatches();
	PatchEngine::ApplyARPatches();
	CoreTiming::ScheduleEvent(VideoInterface::GetTicksPerFrame() - cyclesLate, et_PatchEngine);
}

static void ThrottleCallback(u64 last_time, int cyclesLate)
{
	u32 time = Common::Timer::GetTimeMs();

	int diff = (u32)last_time - time;
	const SConfig& config = SConfig::GetInstance();
	bool frame_limiter = config.m_Framelimit && !Core::GetIsFramelimiterTempDisabled();
	u32 next_event = GetTicksPerSecond()/1000;
	if (SConfig::GetInstance().m_Framelimit > 1)
	{
		next_event = next_event * (SConfig::GetInstance().m_Framelimit - 1) * 5 / VideoInterface::TargetRefreshRate;
	}

	const int max_fallback = 40; // 40 ms for one frame on 25 fps games
	if (frame_limiter && abs(diff) > max_fallback)
	{
		DEBUG_LOG(COMMON, "system too %s, %d ms skipped", diff<0 ? "slow" : "fast", abs(diff) - max_fallback);
		last_time = time - max_fallback;
	}
	else if (frame_limiter && diff > 0)
		Common::SleepCurrentThread(diff);
	CoreTiming::ScheduleEvent(next_event - cyclesLate, et_Throttle, last_time + 1);
}

// split from Init to break a circular dependency between VideoInterface::Init and SystemTimers::Init
void PreInit()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		CPU_CORE_CLOCK = 729000000u;
	else
		CPU_CORE_CLOCK = 486000000u;
}

void Init()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		// AyuanX: TO BE TWEAKED
		// Now the 1500 is a pure assumption
		// We need to figure out the real frequency though

		// FYI, WII_IPC_HLE_Interface::Update is also called in WII_IPCInterface::Write32
		const int freq = 1500;
		IPC_HLE_PERIOD = GetTicksPerSecond() / (freq * VideoInterface::GetNumFields());
	}

	// System internal sample rate is fixed at 32KHz * 4 (16bit Stereo) / 32 bytes DMA
	AUDIO_DMA_PERIOD = CPU_CORE_CLOCK / (AudioInterface::GetAIDSampleRate() * 4 / 32);

	// Emulated gekko <-> flipper bus speed ratio (CPU clock / flipper clock)
	CP_PERIOD = GetTicksPerSecond() / 10000;

	Common::Timer::IncreaseResolution();
	// store and convert localtime at boot to timebase ticks
	CoreTiming::SetFakeTBStartValue((u64)(CPU_CORE_CLOCK / TIMER_RATIO) * (u64)CEXIIPL::GetGCTime());
	CoreTiming::SetFakeTBStartTicks(CoreTiming::GetTicks());

	CoreTiming::SetFakeDecStartValue(0xFFFFFFFF);
	CoreTiming::SetFakeDecStartTicks(CoreTiming::GetTicks());

	et_Dec = CoreTiming::RegisterEvent("DecCallback", DecrementerCallback);
	et_VI = CoreTiming::RegisterEvent("VICallback", VICallback);
	et_SI = CoreTiming::RegisterEvent("SICallback", SICallback);
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSyncGPU)
		et_CP = CoreTiming::RegisterEvent("CPCallback", CPCallback);
	et_DSP = CoreTiming::RegisterEvent("DSPCallback", DSPCallback);
	et_AudioDMA = CoreTiming::RegisterEvent("AudioDMACallback", AudioDMACallback);
	et_IPC_HLE = CoreTiming::RegisterEvent("IPC_HLE_UpdateCallback", IPC_HLE_UpdateCallback);
	et_PatchEngine = CoreTiming::RegisterEvent("PatchEngine", PatchEngineCallback);
	et_Throttle = CoreTiming::RegisterEvent("Throttle", ThrottleCallback);

	CoreTiming::ScheduleEvent(VideoInterface::GetTicksPerLine(), et_VI);
	CoreTiming::ScheduleEvent(0, et_DSP);
	CoreTiming::ScheduleEvent(VideoInterface::GetTicksPerFrame(), et_SI);
	CoreTiming::ScheduleEvent(AUDIO_DMA_PERIOD, et_AudioDMA);
	CoreTiming::ScheduleEvent(0, et_Throttle, Common::Timer::GetTimeMs());
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSyncGPU)
		CoreTiming::ScheduleEvent(CP_PERIOD, et_CP);

	CoreTiming::ScheduleEvent(VideoInterface::GetTicksPerFrame(), et_PatchEngine);

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		CoreTiming::ScheduleEvent(IPC_HLE_PERIOD, et_IPC_HLE);
}

void Shutdown()
{
	Common::Timer::RestoreResolution();
}

}  // namespace
