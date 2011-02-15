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


#ifdef _WIN32
#include <windows.h>
#endif

#include "Setup.h" // Common
#include "Atomic.h"
#include "Thread.h"
#include "Timer.h"
#include "Common.h"
#include "CommonPaths.h"
#include "StringUtil.h"
#include "MathUtil.h"
#include "MemoryUtil.h"

#include "Console.h"
#include "Core.h"
#include "CPUDetect.h"
#include "CoreTiming.h"
#include "Boot/Boot.h"

#include "HW/Memmap.h"
#include "HW/ProcessorInterface.h"
#include "HW/GPFifo.h"
#include "HW/CPU.h"
#include "HW/GCPad.h"
#include "HW/Wiimote.h"
#include "HW/HW.h"
#include "HW/DSP.h"
#include "HW/GPFifo.h"
#include "HW/AudioInterface.h"
#include "HW/VideoInterface.h"
#include "HW/SystemTimers.h"

#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"

#include "PowerPC/PowerPC.h"
#include "PowerPC/JitCommon/JitBase.h"

#include "DSPEmulator.h"
#include "ConfigManager.h"
#include "VideoBackendBase.h"

#include "VolumeHandler.h"
#include "FileMonitor.h"

#include "MemTools.h"
#include "Host.h"
#include "LogManager.h"

#include "State.h"
#include "OnFrame.h"

// TODO: ugly, remove
bool g_aspect_wide;

namespace Core
{

// Declarations and definitions
Common::Timer Timer;
volatile u32 DrawnFrame = 0;
u32 DrawnVideo = 0;

// Function forwarding
void Callback_DSPLog(const TCHAR* _szMessage, int _v);
const char *Callback_ISOName(void);
void Callback_DSPInterrupt();
void Callback_WiimoteInterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);

// Function declarations
void EmuThread();

void Stop();

bool g_bStopping = false;
bool g_bHwInit = false;
bool g_bRealWiimote = false;
void *g_pWindowHandle = NULL;
std::string g_stateFileName;
std::thread g_EmuThread;

static std::thread cpuThread;

SCoreStartupParameter g_CoreStartupParameter;

// This event is set when the emuthread starts.
Common::Event emuThreadGoing;
Common::Event cpuRunloopQuit;

std::string GetStateFileName() { return g_stateFileName; }
void SetStateFileName(std::string val) { g_stateFileName = val; }

// Display messages and return values

// Formatted stop message
std::string StopMessage(bool bMainThread, std::string Message)
{
	return StringFromFormat("Stop [%s %i]\t%s\t%s",
		bMainThread ? "Main Thread" : "Video Thread", Common::CurrentThreadId(), MemUsage().c_str(), Message.c_str());
}

// 
bool PanicAlertToVideo(const char* text, bool yes_no)
{
	DisplayMessage(text, 3000);
	return true;
}

void DisplayMessage(const std::string &message, int time_in_ms)
{
	SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	g_video_backend->Video_AddMessage(message.c_str(), time_in_ms);
	if (_CoreParameter.bRenderToMain &&
		SConfig::GetInstance().m_InterfaceStatusbar) {
			Host_UpdateStatusBar(message.c_str());
	} else
		Host_UpdateTitle(message.c_str());
}

void DisplayMessage(const char *message, int time_in_ms)
{
	SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	g_video_backend->Video_AddMessage(message, time_in_ms);
	if (_CoreParameter.bRenderToMain &&
		SConfig::GetInstance().m_InterfaceStatusbar) {
			Host_UpdateStatusBar(message);
	} else
		Host_UpdateTitle(message);
}

void Callback_DebuggerBreak()
{
	CCPU::Break();
}

void *GetWindowHandle()
{
	return g_pWindowHandle;
}

bool GetRealWiimote()
{
	return g_bRealWiimote;
}

bool isRunning()
{
	return (GetState() != CORE_UNINITIALIZED) || g_bHwInit;
}

bool IsRunningInCurrentThread()
{
	return isRunning() && ((!cpuThread.joinable()) || cpuThread.get_id() == std::this_thread::get_id());
}

bool IsCPUThread()
{
	return ((!cpuThread.joinable()) || cpuThread.get_id() == std::this_thread::get_id());
}

// This is called from the GUI thread. See the booting call schedule in
// BootManager.cpp
bool Init()
{
	if (g_EmuThread.joinable())
	{
		PanicAlertT("Emu Thread already running");
		return false;
	}

	g_CoreStartupParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	// FIXME DEBUG_LOG(BOOT, dump_params());
	Host_SetWaitCursor(true);

	emuThreadGoing.Init();

	// Start the emu thread 
	g_EmuThread = std::thread(EmuThread);

	// Wait until the emu thread is running
	emuThreadGoing.MsgWait();
	emuThreadGoing.Shutdown();

	Host_SetWaitCursor(false);
	return true;
}

// Called from GUI thread
void Stop()  // - Hammertime!
{
	const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;
	g_bStopping = true;
	g_video_backend->EmuStateChange(EMUSTATE_CHANGE_STOP);

	INFO_LOG(CONSOLE, "Stop [Main Thread]\t\t---- Shutting down ----");	

	Host_SetWaitCursor(true);  // hourglass!
	if (PowerPC::GetState() == PowerPC::CPU_POWERDOWN)
		return;

	// Stop the CPU
	INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stop CPU").c_str());
	PowerPC::Stop();
	CCPU::StepOpcode();  // Kick it if it's waiting (code stepping wait loop)

	if (_CoreParameter.bCPUThread)
	{
		// Video_EnterLoop() should now exit so that EmuThread() will continue
		// concurrently with the rest of the commands in this function. We no
		// longer rely on Postmessage.
		INFO_LOG(CONSOLE, "%s", StopMessage(true, "Wait for Video Loop to exit ...").c_str());
		g_video_backend->Video_ExitLoop();

		// Wait until the CPU finishes exiting the main run loop
		cpuRunloopQuit.Wait();
	}

	// Clear on screen messages that haven't expired
	g_video_backend->Video_ClearMessages();

	// Close the trace file
	Core::StopTrace();

	// Update mouse pointer
	Host_SetWaitCursor(false);

	INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stopping Emu thread ...").c_str());
	g_EmuThread.join();	// Wait for emuthread to close. 
}

// Create the CPU thread. which would be a CPU + Video thread in Single Core mode.

void CpuThread()
{
	const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	if (_CoreParameter.bCPUThread)
	{
		Common::SetCurrentThreadName("CPU thread");
	}
	else
	{
		g_video_backend->Video_Prepare();
		Common::SetCurrentThreadName("CPU-GPU thread");
	}

	if (_CoreParameter.bLockThreads)
		Common::SetCurrentThreadAffinity(1);  // Force to first core

	if (_CoreParameter.bUseFastMem)
	{
		#ifdef _M_X64
			// Let's run under memory watch
			EMM::InstallExceptionHandler();
		#else
			PanicAlertT("32-bit platforms do not support fastmem yet. Report this bug.");
		#endif
	}

	if (!g_stateFileName.empty())
		State_LoadAs(g_stateFileName);

	// Enter CPU run loop. When we leave it - we are done.
	CCPU::Run();

	// The shutdown function of OpenGL is not thread safe
	// So we have to call the shutdown from the thread that started it.
	if (!_CoreParameter.bCPUThread)
	{
		g_video_backend->Shutdown();
	}

	cpuRunloopQuit.Set();

	return;
}


// Initalize and create emulation thread
// Call browser: Init():g_EmuThread(). See the BootManager.cpp file description for a complete call schedule.
void EmuThread()
{
	Host_UpdateMainFrame(); // Disable any menus or buttons at boot

	cpuRunloopQuit.Init();

	Common::SetCurrentThreadName("Emuthread - starting");
	const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	if (_CoreParameter.bLockThreads)
	{
		if (cpu_info.num_cores > 3)
			Common::SetCurrentThreadAffinity(4);  // Force to third, non-HT core
		else
			Common::SetCurrentThreadAffinity(2);  // Force to second core
	}

	INFO_LOG(OSREPORT, "Starting core = %s mode", _CoreParameter.bWii ? "Wii" : "Gamecube");
	INFO_LOG(OSREPORT, "CPU Thread separate = %s", _CoreParameter.bCPUThread ? "Yes" : "No");

	HW::Init();	

	emuThreadGoing.Set();

	g_aspect_wide = _CoreParameter.bWii;
	if (g_aspect_wide) 
	{
		IniFile gameIni;
		gameIni.Load(_CoreParameter.m_strGameIni.c_str());
		gameIni.Get("Wii", "Widescreen", &g_aspect_wide, !!SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.AR"));
	}

	// _CoreParameter.hMainWindow is first the m_Panel handle, then it is updated to have the new window handle,
	// within g_video_backend->Initialize()
	// TODO: that's ugly, change Initialize() to take m_Panel and return the new window handle
	g_video_backend->Initialize();
	g_pWindowHandle = _CoreParameter.hMainWindow;

	DSP::GetDSPEmulator()->Initialize(g_pWindowHandle, _CoreParameter.bWii, _CoreParameter.bDSPThread);
	
	Pad::Initialize(g_pWindowHandle);

	// Load and Init Wiimotes - only if we are booting in wii mode	
	if (_CoreParameter.bWii)
	{
		Wiimote::Initialize(g_pWindowHandle);

		// activate wiimotes which don't have source set to "None"
		for (unsigned int i = 0; i != MAX_WIIMOTES; ++i)
			if (g_wiimote_sources[i])
				GetUsbPointer()->AccessWiiMote(i | 0x100)->Activate(true);
	}

	// The hardware is initialized.
	g_bHwInit = true;

	DisplayMessage("CPU: " + cpu_info.Summarize(), 8000);
	DisplayMessage(_CoreParameter.m_strFilename, 3000);

	// Load GCM/DOL/ELF whatever ... we boot with the interpreter core
	PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
	CBoot::BootUp();

	// Setup our core, but can't use dynarec if we are compare server
	if (_CoreParameter.iCPUCore && (!_CoreParameter.bRunCompareServer || _CoreParameter.bRunCompareClient))
		PowerPC::SetMode(PowerPC::MODE_JIT);
	else
		PowerPC::SetMode(PowerPC::MODE_INTERPRETER);

	// Spawn the CPU thread
	_dbg_assert_(OSHLE, !cpuThread.joinable());
	// ENTER THE VIDEO THREAD LOOP
	if (_CoreParameter.bCPUThread)
	{
		// This thread, after creating the EmuWindow, spawns a CPU thread,
		// and then takes over and becomes the video thread

		g_video_backend->Video_Prepare(); // wglMakeCurrent
		cpuThread = std::thread(CpuThread);
		Common::SetCurrentThreadName("Video thread");

		// Update the window again because all stuff is initialized
		Host_UpdateDisasmDialog();
		Host_UpdateMainFrame();

		g_video_backend->Video_EnterLoop();
	}
	else // SingleCore mode
	{
		// the spawned CPU Thread also does the graphics.  the EmuThread is
		// thus an idle thread, which sleep wait for the program to terminate.
		// Without this extra thread, the video backend window hangs in single
		// core mode since noone is pumping messages.

		cpuThread = std::thread(CpuThread);
		Common::SetCurrentThreadName("Emuthread - Idle");

		// Update the window again because all stuff is initialized
		Host_UpdateDisasmDialog();
		Host_UpdateMainFrame();

		// TODO(ector) : investigate using GetMessage instead .. although
		// then we lose the powerdown check. ... unless powerdown sends a message :P
		while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
		{
			g_video_backend->PeekMessages();
			Common::SleepCurrentThread(20);
		}

		// Wait for CpuThread to exit
		INFO_LOG(CONSOLE, "%s", StopMessage(true, "Stopping CPU-GPU thread ...").c_str());
		cpuRunloopQuit.Wait();
		INFO_LOG(CONSOLE, "%s", StopMessage(true, "CPU thread stopped.").c_str());
		// On unix platforms, the Emulation main thread IS the CPU & video
		// thread So there's only one thread, imho, that's much better than on
		// windows :P
		//CpuThread(pArg);
	}

	// We have now exited the Video Loop
	INFO_LOG(CONSOLE, "%s", StopMessage(false, "Stop() and Video Loop Ended").c_str());

	// At this point, the CpuThread has already returned in SC mode.
	// But it may still be waiting in Dual Core mode.
	if (cpuThread.joinable())
	{
		// There is a CPU thread - join it.
		cpuThread.join();
	}

	VolumeHandler::EjectVolume();
	FileMon::Close();

	// Stop audio thread - Actually this does nothing when using HLE emulation.
	// But stops the DSP Interpreter when using LLE emulation.
	DSP::GetDSPEmulator()->DSP_StopSoundStream();
	
	// We must set up this flag before executing HW::Shutdown()
	g_bHwInit = false;
	INFO_LOG(CONSOLE, "%s", StopMessage(false, "Shutting down HW").c_str());
	HW::Shutdown();
	INFO_LOG(CONSOLE, "%s", StopMessage(false, "HW shutdown").c_str());

	// In single core mode, this has already been called.
	if (_CoreParameter.bCPUThread)
		g_video_backend->Shutdown();

	Pad::Shutdown();
	Wiimote::Shutdown();

	INFO_LOG(CONSOLE, "%s", StopMessage(true, "Main thread stopped").c_str());
	INFO_LOG(CONSOLE, "Stop [Main Thread]\t\t---- Shutdown complete ----");

	cpuRunloopQuit.Shutdown();
	g_bStopping = false;
}

// Set or get the running state

void SetState(EState _State)
{
	switch (_State)
	{
	case CORE_UNINITIALIZED:
		Stop();
		break;
	case CORE_PAUSE:
		CCPU::EnableStepping(true);  // Break
		break;
	case CORE_RUN:
		CCPU::EnableStepping(false);
		break;
	default:
		PanicAlertT("Invalid state");
		break;
	}
}

EState GetState()
{
	if (g_bHwInit)
	{
		if (CCPU::IsStepping())
			return CORE_PAUSE;
		else if (g_bStopping)
			return CORE_STOPPING;
		else
			return CORE_RUN;
	}
	return CORE_UNINITIALIZED;
}

static inline std::string GenerateScreenshotName()
{
	int index = 1;
	std::string tempname, name;
	std::string gameId = SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID();
	tempname = std::string(File::GetUserPath(D_SCREENSHOTS_IDX)) + gameId + DIR_SEP_CHR;

	if (!File::CreateFullPath(tempname.c_str())) {
		//fallback to old-style screenshots, without folder.
		tempname = std::string(File::GetUserPath(D_SCREENSHOTS_IDX));
	}
	//append gameId, tempname only contains the folder here.
	tempname += gameId;

	do
		name = StringFromFormat("%s-%d.png", tempname.c_str(), index++);
	while (File::Exists(name.c_str()));

	return name;
}

void ScreenShot(const std::string& name)
{
	bool bPaused = (GetState() == CORE_PAUSE);

	SetState(CORE_PAUSE);
	g_video_backend->Video_Screenshot(name.c_str());
	if(!bPaused)
		SetState(CORE_RUN);
}

void ScreenShot()
{
	ScreenShot(GenerateScreenshotName());
}

// Apply Frame Limit and Display FPS info
// This should only be called from VI
void VideoThrottle()
{
	u32 TargetVPS = (SConfig::GetInstance().m_Framelimit > 1) ?
		SConfig::GetInstance().m_Framelimit * 5 : VideoInterface::TargetRefreshRate;

	// Disable the frame-limiter when the throttle (Tab) key is held down
	if (SConfig::GetInstance().m_Framelimit && !Host_GetKeyState('\t'))
	{
		u32 frametime = ((SConfig::GetInstance().b_UseFPS)? Common::AtomicLoad(DrawnFrame) : DrawnVideo) * 1000 / TargetVPS;

		u32 timeDifference = (u32)Timer.GetTimeDifference();
		if (timeDifference < frametime) {
			Common::SleepCurrentThread(frametime - timeDifference - 1);
		}

		while ((u32)Timer.GetTimeDifference() < frametime)
			Common::YieldCPU();
			//Common::SleepCurrentThread(1);
	}

	// Update info per second
	u32 ElapseTime = (u32)Timer.GetTimeDifference();
	if ((ElapseTime >= 1000 && DrawnVideo > 0) || Frame::g_bFrameStep)
	{
		SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

		u32 FPS = Common::AtomicLoad(DrawnFrame) * 1000 / ElapseTime;
		u32 VPS = DrawnVideo * 1000 / ElapseTime;
		u32 Speed = VPS * 100 / VideoInterface::TargetRefreshRate;
		
		// Settings are shown the same for both extended and summary info
		std::string SSettings = StringFromFormat("%s %s", cpu_core_base->GetName(),	_CoreParameter.bCPUThread ? "DC" : "SC");

		// Use extended or summary information. The summary information does not print the ticks data,
		// that's more of a debugging interest, it can always be optional of course if someone is interested.
		//#define EXTENDED_INFO
		#ifdef EXTENDED_INFO
			u64 newTicks = CoreTiming::GetTicks();
			u64 newIdleTicks = CoreTiming::GetIdleTicks();
	 
			u64 diff = (newTicks - ticks) / 1000000;
			u64 idleDiff = (newIdleTicks - idleTicks) / 1000000;
	 
			ticks = newTicks;
			idleTicks = newIdleTicks;	 
			
			float TicksPercentage = (float)diff / (float)(SystemTimers::GetTicksPerSecond() / 1000000) * 100;

			std::string SFPS = StringFromFormat("FPS: %u - VPS: %u - SPEED: %u%%", FPS, VPS, Speed);
			SFPS += StringFromFormat(" | CPU: %s%i MHz [Real: %i + IdleSkip: %i] / %i MHz (%s%3.0f%%)",
					_CoreParameter.bSkipIdle ? "~" : "",
					(int)(diff),
					(int)(diff - idleDiff),
					(int)(idleDiff),
					SystemTimers::GetTicksPerSecond() / 1000000,
					_CoreParameter.bSkipIdle ? "~" : "",
					TicksPercentage);

		#else	// Summary information
		std::string SFPS;
		if (Frame::g_recordfd)
			SFPS = StringFromFormat("Frame: %d | FPS: %u - VPS: %u - SPEED: %u%%", Frame::g_frameCounter, FPS, VPS, Speed);
		else
			SFPS = StringFromFormat("FPS: %u - VPS: %u - SPEED: %u%%", FPS, VPS, Speed);
		#endif

		// This is our final "frame counter" string
		std::string SMessage = StringFromFormat("%s | %s",
			SSettings.c_str(), SFPS.c_str());
		std::string TMessage = StringFromFormat("%s | ", svn_rev_str) +
			SMessage;

		// Show message
		g_video_backend->UpdateFPSDisplay(SMessage.c_str()); 

		if (_CoreParameter.bRenderToMain &&
			SConfig::GetInstance().m_InterfaceStatusbar) {
			Host_UpdateStatusBar(SMessage.c_str());
			Host_UpdateTitle(svn_rev_str);
		} else
			Host_UpdateTitle(TMessage.c_str());
		

		// Reset counter
		Timer.Update();
		Common::AtomicStore(DrawnFrame, 0);
		DrawnVideo = 0;
	}

	DrawnVideo++;
}

// Executed from GPU thread
// reports if a frame should be skipped or not
// depending on the framelimit set
bool report_slow(int skipped)
{
	u32 TargetFPS = (SConfig::GetInstance().m_Framelimit > 1) ? SConfig::GetInstance().m_Framelimit * 5
		: VideoInterface::TargetRefreshRate;
	u32 frames = Common::AtomicLoad(DrawnFrame);
	bool fps_slow = (Timer.GetTimeDifference() < (frames + skipped) * 1000 / TargetFPS) ? false : true;

	return fps_slow;
}

// --- Callbacks for backends / engine ---

// Callback_VideoLog
// WARNING - THIS IS EXECUTED FROM VIDEO THREAD
void Callback_VideoLog(const char *_szMessage)
{
	INFO_LOG(VIDEO, "%s", _szMessage);
}

// Should be called from GPU thread when a frame is drawn
void Callback_VideoCopiedToXFB(bool video_update)
{
	if(video_update)
		Common::AtomicIncrement(DrawnFrame);
	Frame::FrameUpdate();
}

// Ask the host for the window size
void Callback_VideoGetWindowSize(int& x, int& y, int& width, int& height)
{
	Host_GetRenderWindowSize(x, y, width, height);
}

// Suggest to the host that it sets the window to the given size.
// The host may or may not decide to do this depending on fullscreen or not.
// Sets width and height to the actual size of the window.
void Callback_VideoRequestWindowSize(int& width, int& height)
{
	Host_RequestRenderWindowSize(width, height);
}

// Callback_DSPLog
// WARNING - THIS MAY BE EXECUTED FROM DSP THREAD
void Callback_DSPLog(const TCHAR* _szMessage, int _v)
{
	GENERIC_LOG(LogTypes::AUDIO, (LogTypes::LOG_LEVELS)_v, "%s", _szMessage);
}


// Callback_DSPInterrupt
// WARNING - THIS MAY BE EXECUTED FROM DSP THREAD
void Callback_DSPInterrupt()
{
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}


// Callback_ISOName: Let the DSP emulator get the game name
//
const char *Callback_ISOName()
{
	SCoreStartupParameter& params = SConfig::GetInstance().m_LocalCoreStartupParameter;
	if (params.m_strName.length() > 0)
		return params.m_strName.c_str();
	else	
		return "";
}

// Called from ANY thread!
// Pass the message on to the host
void Callback_CoreMessage(int Id)
{
	Host_Message(Id);
}

} // Core
