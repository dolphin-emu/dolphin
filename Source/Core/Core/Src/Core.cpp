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
#include "Thread.h"
#include "Timer.h"
#include "Common.h"
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
#include "HW/HW.h"
#include "HW/DSP.h"
#include "HW/GPFifo.h"
#include "HW/AudioInterface.h"
#include "HW/VideoInterface.h"
#include "HW/CommandProcessor.h"
#include "HW/PixelEngine.h"
#include "HW/SystemTimers.h"
 
#include "PowerPC/PowerPC.h"
 
#include "PluginManager.h"
#include "ConfigManager.h"
 
#include "MemTools.h"
#include "Host.h"
#include "LogManager.h"
 
#include "State.h"
#include "OnFrame.h"
 
#ifndef _WIN32
#define WINAPI
#endif

namespace Core
{
 

// Declarations and definitions
Common::Timer Timer;
u32 frames = 0;

 
// Function forwarding
//void Callback_VideoRequestWindowSize(int _iWidth, int _iHeight, BOOL _bFullscreen);
void Callback_VideoLog(const TCHAR* _szMessage, int _bDoBreak);
void Callback_VideoCopiedToXFB(bool video_update);
void Callback_DSPLog(const TCHAR* _szMessage, int _v);
const char *Callback_ISOName(void);
void Callback_DSPInterrupt();
void Callback_PADLog(const TCHAR* _szMessage);
void Callback_WiimoteLog(const TCHAR* _szMessage, int _v);
void Callback_WiimoteInput(u16 _channelID, const void* _pData, u32 _Size);
 
// For keyboard shortcuts.
void Callback_KeyPress(int key, bool shift, bool control);
TPeekMessages Callback_PeekMessages = NULL;
TUpdateFPSDisplay g_pUpdateFPSDisplay = NULL;
 
// Function declarations
THREAD_RETURN EmuThread(void *pArg);

void Stop();
 
bool g_bStopping = false;
bool g_bHwInit = false;
bool g_bRealWiimote = false;
HWND g_pWindowHandle = NULL;
Common::Thread* g_EmuThread = NULL;

SCoreStartupParameter g_CoreStartupParameter;

// This event is set when the emuthread starts.
Common::Event emuThreadGoing;
Common::Event cpuRunloopQuit;
Common::Event gpuShutdownCall;
 

// Display messages and return values

// Formatted stop message
std::string StopMessage(bool bMainThread, std::string Message)
{
	return StringFromFormat("Stop [%s %i]\t%s\t%s",
		bMainThread ? "Main Thread" : "Video Thread", Common::Thread::CurrentId(), MemUsage().c_str(), Message.c_str());
}

// 
bool PanicAlertToVideo(const char* text, bool yes_no)
{
	DisplayMessage(text, 3000);
	return true;
}
 
void DisplayMessage(const std::string &message, int time_in_ms)
{
    CPluginManager::GetInstance().GetVideo()->Video_AddMessage(message.c_str(),
							       time_in_ms);
}
 
void DisplayMessage(const char *message, int time_in_ms)
{
    CPluginManager::GetInstance().GetVideo()->Video_AddMessage(message, 
							    time_in_ms);
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

// This can occur when the emulator is not running and the nJoy configuration window is opened
void ReconnectPad()
{
	CPluginManager &Plugins = CPluginManager::GetInstance();
	Plugins.FreePad(0);
	Plugins.GetPad(0)->Config(g_pWindowHandle);
	INFO_LOG(CONSOLE, "ReconnectPad()\n");
}

// This doesn't work yet, I don't understand how the connection work yet
void ReconnectWiimote()
{
	// This seems to be a hack that just sets some IPC registers to zero. Dubious.
	/* JP: Yes, it's basically nothing right now, I could not figure out how to reset the Wiimote
	   for reconnection */
	HW::InitWiimote();
	INFO_LOG(CONSOLE, "ReconnectWiimote()\n");
}

bool isRunning()
{
	return (GetState() != CORE_UNINITIALIZED) || g_bHwInit;
}


// This is called from the GUI thread. See the booting call schedule in BootManager.cpp

bool Init()
{
	if (g_EmuThread != NULL)
	{
		PanicAlert("ERROR: Emu Thread already running. Report this bug.");
		return false;
	}

	Common::InitThreading();

	// Get a handle to the current instance of the plugin manager
	CPluginManager &pManager = CPluginManager::GetInstance();
	SCoreStartupParameter &_CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	g_CoreStartupParameter = _CoreParameter;
	// FIXME DEBUG_LOG(BOOT, dump_params());
	Host_SetWaitCursor(true);

	// Start the thread again
	_dbg_assert_(HLE, g_EmuThread == NULL);

	// Check that all plugins exist, potentially call LoadLibrary() for unloaded plugins
	if (!pManager.InitPlugins())
		return false;

	emuThreadGoing.Init();
	// This will execute EmuThread() further down in this file
	g_EmuThread = new Common::Thread(EmuThread, NULL);
	
	emuThreadGoing.MsgWait();
	emuThreadGoing.Shutdown();

	// All right, the event is set and killed. We are now running.
	Host_SetWaitCursor(false);
	return true;
}

// Called from GUI thread or VI thread (why VI??? That must be bad. Window close? TODO: Investigate.)
void Stop()  // - Hammertime!
{
	const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;
	g_bStopping = true;

	WARN_LOG(CONSOLE, "Stop [Main Thread]\t\t---- Shutting down ----");	

	// This must be done a while before freeing the dll to not crash wx around MSWWindowProc and DefWindowProc, will investigate further
	Host_Message(AUDIO_DESTROY);
	Host_Message(VIDEO_DESTROY);

	Host_SetWaitCursor(true);  // hourglass!
	if (PowerPC::GetState() == PowerPC::CPU_POWERDOWN)
		return;

	WARN_LOG(CONSOLE, "%s", StopMessage(true, "Stop CPU").c_str());

	// Stop the CPU
	PowerPC::Stop();
	CCPU::StepOpcode();  // Kick it if it's waiting (code stepping wait loop)

	// Wait until the CPU finishes exiting the main run loop
	cpuRunloopQuit.Wait();
	cpuRunloopQuit.Shutdown();
	// At this point, we must be out of the CPU:s runloop.

	// Stop audio thread.
	CPluginManager::GetInstance().GetDSP()->DSP_StopSoundStream();
 
	// If dual core mode, the CPU thread should immediately exit here.
	if (_CoreParameter.bUseDualCore) {
		NOTICE_LOG(CONSOLE, "%s", StopMessage(true, "Wait for Video Loop to exit ...").c_str());
		CPluginManager::GetInstance().GetVideo()->Video_ExitLoop();
	}

	// Video_EnterLoop() should now exit so that EmuThread() will continue concurrently with the rest
	// of the commands in this function. We no longer rely on Postmessage. 

	// Close the trace file
	Core::StopTrace();
	WARN_LOG(CONSOLE, "%s", StopMessage(true, "Shutting down core").c_str());

	// Update mouse pointer
	Host_SetWaitCursor(false);

	WARN_LOG(CONSOLE, "%s", StopMessage(true, "Stopping Emu thread ...").c_str());
#ifdef _WIN32
	DWORD Wait = g_EmuThread->WaitForDeath(5000);
	switch(Wait)
	{
		case WAIT_ABANDONED:
			ERROR_LOG(CONSOLE, "%s", StopMessage(true, "Emu wait returned: WAIT_ABANDONED").c_str());
			break;
		case WAIT_OBJECT_0:
			NOTICE_LOG(CONSOLE, "%s", StopMessage(true, "Emu wait returned: WAIT_OBJECT_0").c_str());
			break;
		case WAIT_TIMEOUT:
			ERROR_LOG(CONSOLE, "%s", StopMessage(true, "Emu wait returned: WAIT_TIMEOUT").c_str());
			break;				
		case WAIT_FAILED:
			ERROR_LOG(CONSOLE, "%s", StopMessage(true, "Emu wait returned: WAIT_FAILED").c_str());
			break;
	}
#else
	g_EmuThread->WaitForDeath();
#endif
	delete g_EmuThread;  // Wait for emuthread to close.
	g_EmuThread = 0;
}

 

// Create the CPU thread. which would be a CPU + Video thread in Single Core mode.

THREAD_RETURN CpuThread(void *pArg)
{
	CPluginManager &Plugins = CPluginManager::GetInstance();
	const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	if (_CoreParameter.bUseDualCore)
	{
		Common::SetCurrentThreadName("CPU thread");
	}
	else
	{
		CPluginManager::GetInstance().GetVideo()->Video_Prepare();
		Common::SetCurrentThreadName("CPU-GPU thread");
	}
 
	if (_CoreParameter.bLockThreads)
		Common::Thread::SetCurrentThreadAffinity(1);  // Force to first core
 
	if (_CoreParameter.bUseFastMem)
	{
		#ifdef _M_X64
			// Let's run under memory watch
			#ifndef JITTEST
			EMM::InstallExceptionHandler();
			#endif
		#else
			PanicAlert("32-bit platforms do not support fastmem yet. Report this bug.");
		#endif
	}
 
	// Enter CPU run loop. When we leave it - we are done.
	CCPU::Run();
	cpuRunloopQuit.Set();

#ifdef _WIN32
	gpuShutdownCall.Wait();

	// Call video shutdown from the video thread in single core mode, which is the cpuThread
	if (!_CoreParameter.bUseDualCore)
		Plugins.ShutdownVideoPlugin();
#endif

	gpuShutdownCall.Shutdown();

 	return 0;
}


// Initalize plugins and create emulation thread
// Call browser: Init():g_EmuThread(). See the BootManager.cpp file description for a complete call schedule.
THREAD_RETURN EmuThread(void *pArg)
{
	cpuRunloopQuit.Init();
	gpuShutdownCall.Init();

	Common::SetCurrentThreadName("Emuthread - starting");
	const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	CPluginManager &Plugins = CPluginManager::GetInstance();
	if (_CoreParameter.bLockThreads)
		Common::Thread::SetCurrentThreadAffinity(2);  // Force to second core
 
	INFO_LOG(OSREPORT, "Starting core = %s mode", _CoreParameter.bWii ? "Wii" : "Gamecube");
	INFO_LOG(OSREPORT, "Dualcore = %s", _CoreParameter.bUseDualCore ? "Yes" : "No");

	HW::Init();	

	emuThreadGoing.Set();
 
	// Load the VideoPlugin
 	SVideoInitialize VideoInitialize;
	VideoInitialize.pGetMemoryPointer	= Memory::GetPointer;
	VideoInitialize.pSetPEToken			= PixelEngine::SetToken;
	VideoInitialize.pSetPEFinish		= PixelEngine::SetFinish;
	// This is first the m_Panel handle, then it is updated to have the new window handle
	VideoInitialize.pWindowHandle		= _CoreParameter.hMainWindow;
	VideoInitialize.pLog				= Callback_VideoLog;
	VideoInitialize.pSysMessage			= Host_SysMessage;
	VideoInitialize.pRequestWindowSize	= NULL; //Callback_VideoRequestWindowSize;
	VideoInitialize.pCopiedToXFB		= Callback_VideoCopiedToXFB;
	VideoInitialize.pPeekMessages       = NULL;
	VideoInitialize.pUpdateFPSDisplay   = NULL;
	VideoInitialize.pCPFifo             = (SCPFifoStruct*)&CommandProcessor::fifo;
	VideoInitialize.pUpdateInterrupts   = &(CommandProcessor::UpdateInterruptsFromVideoPlugin);
	VideoInitialize.pMemoryBase         = Memory::base;
	VideoInitialize.pKeyPress           = Callback_KeyPress;
	VideoInitialize.pSetFifoIdle        = &(CommandProcessor::SetFifoIdleFromVideoPlugin);
	VideoInitialize.bWii                = _CoreParameter.bWii;
	VideoInitialize.bUseDualCore		= _CoreParameter.bUseDualCore;
	VideoInitialize.pBBox               = &PixelEngine::bbox[0];
	VideoInitialize.pBBoxActive         = &PixelEngine::bbox_active;

	Plugins.GetVideo()->Initialize(&VideoInitialize); // Call the dll
 
	// Under linux, this is an X11 Display, not an HWND!
	g_pWindowHandle = (HWND)VideoInitialize.pWindowHandle;
	Callback_PeekMessages = VideoInitialize.pPeekMessages;
	g_pUpdateFPSDisplay = VideoInitialize.pUpdateFPSDisplay;

    // Load and init DSPPlugin	
	DSPInitialize dspInit;
	dspInit.hWnd = g_pWindowHandle;
	dspInit.pARAM_Read_U8 = (u8  (__cdecl *)(const u32))DSP::ReadARAM; 
	dspInit.pARAM_Write_U8 = (void (__cdecl *)(const u8, const u32))DSP::WriteARAM; 
	dspInit.pGetARAMPointer = DSP::GetARAMPtr;
	dspInit.pGetMemoryPointer = Memory::GetPointer;
	dspInit.pLog = Callback_DSPLog;
	dspInit.pName = Callback_ISOName;
	dspInit.pDebuggerBreak = Callback_DebuggerBreak;
	dspInit.pGenerateDSPInterrupt = Callback_DSPInterrupt;
	dspInit.pGetAudioStreaming = AudioInterface::Callback_GetStreaming;
	dspInit.pEmulatorState = (int *)PowerPC::GetStatePtr();
	dspInit.bWii = _CoreParameter.bWii;
	dspInit.bOnThread = _CoreParameter.bDSPThread;

	Plugins.GetDSP()->Initialize((void *)&dspInit);

	// Load and Init PadPlugin
	for (int i = 0; i < MAXPADS; i++)
	{			
		SPADInitialize PADInitialize;
		PADInitialize.hWnd = g_pWindowHandle;
		PADInitialize.pLog = Callback_PADLog;
		PADInitialize.padNumber = i;
		// This is may be needed to avoid a SDL problem
		//Plugins.FreeWiimote();
		// Check if we should init the plugin
		if (Plugins.OkayToInitPlugin(i))
		{
			Plugins.GetPad(i)->Initialize(&PADInitialize);

			// Check if joypad open failed, in that case try again
			if (PADInitialize.padNumber == -1)
			{
				Plugins.GetPad(i)->Shutdown();
				Plugins.FreePad(i);
				Plugins.GetPad(i)->Initialize(&PADInitialize);
			}
		}
	}

	// Load and Init WiimotePlugin - only if we are booting in wii mode	
	if (_CoreParameter.bWii)
	{
		SWiimoteInitialize WiimoteInitialize;
		WiimoteInitialize.hWnd = g_pWindowHandle;
		// Add the ISO Id
		WiimoteInitialize.ISOId = Ascii2Hex(_CoreParameter.m_strUniqueID);
		WiimoteInitialize.pLog = Callback_WiimoteLog;
		WiimoteInitialize.pWiimoteInput = Callback_WiimoteInput;
		// Wait for Wiiuse to find the number of connected Wiimotes
		Plugins.GetWiimote(0)->Initialize((void *)&WiimoteInitialize);
	}
 
	// The hardware is initialized.
	g_bHwInit = true;

	DisplayMessage("CPU: " + cpu_info.Summarize(), 8000);
	DisplayMessage(_CoreParameter.m_strFilename, 3000);
 
	// Load GCM/DOL/ELF whatever ... we boot with the interpreter core
	PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
	CBoot::BootUp();
 
	if (g_pUpdateFPSDisplay != NULL)
        g_pUpdateFPSDisplay(("Loading " + _CoreParameter.m_strFilename).c_str());
 
	// Setup our core, but can't use dynarec if we are compare server
	if (_CoreParameter.bUseJIT && (!_CoreParameter.bRunCompareServer || _CoreParameter.bRunCompareClient))
		PowerPC::SetMode(PowerPC::MODE_JIT);
	else
		PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
 
	// Update the window again because all stuff is initialized
	Host_UpdateDisasmDialog();
	Host_UpdateMainFrame();

	// Spawn the CPU thread
	Common::Thread *cpuThread = NULL;
 
	// ENTER THE VIDEO THREAD LOOP
	if (_CoreParameter.bUseDualCore) // DualCore mode
	{
		// This thread, after creating the EmuWindow, spawns a CPU thread,
		// and then takes over and becomes the video thread

		Plugins.GetVideo()->Video_Prepare(); // wglMakeCurrent
		cpuThread = new Common::Thread(CpuThread, pArg);
		Common::SetCurrentThreadName("Video thread");

		Plugins.GetVideo()->Video_EnterLoop();
	}
	else // SingleCore mode
	{
#ifdef _WIN32
		// the spawned CPU Thread is the... CPU thread but it also does the graphics.
		// the EmuThread is thus an idle thread, which sleeps and wait for the emu to terminate.
		// Without this extra thread, the video plugin window hangs in single core mode since
		// noone is pumping messages.

		cpuThread = new Common::Thread(CpuThread, pArg);
		Common::SetCurrentThreadName("Emuthread - Idle");

		// TODO(ector) : investigate using GetMessage instead .. although
		// then we lose the powerdown check. ... unless powerdown sends a message :P
		while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
		{
			if (Callback_PeekMessages)
				Callback_PeekMessages();
			Common::SleepCurrentThread(20);
		}
#else
		// On unix platforms, the Emulation main thread IS the CPU & video thread
		// So there's only one thread, imho, that's much better than on windows :P
		CpuThread(pArg);
#endif
	}

	NOTICE_LOG(CONSOLE, "%s", StopMessage(false, "Stop() and Video Loop Ended").c_str());
	WARN_LOG(CONSOLE, "%s", StopMessage(false, "Shutting down HW").c_str());

	// We have now exited the Video Loop and will shut down

	// Write message
	if (g_pUpdateFPSDisplay != NULL) g_pUpdateFPSDisplay("Stopping...");

	HW::Shutdown();

	NOTICE_LOG(CONSOLE, "%s", StopMessage(false, "HW shutdown").c_str());
	WARN_LOG(CONSOLE, "%s", StopMessage(false, "Shutting down plugins").c_str());
	Plugins.ShutdownPlugins();
	NOTICE_LOG(CONSOLE, "%s", StopMessage(false, "Plugins shutdown").c_str());

#ifdef _WIN32
	gpuShutdownCall.Set();

	// Call video shutdown from the video thread, in dual core mode it's the EmuThread
	// Or set an event in Single Core mode, to call the shutdown from the cpuThread
	if (_CoreParameter.bUseDualCore)
		Plugins.ShutdownVideoPlugin();
#else
	// On unix platforms, the EmuThread is ALWAYS the video thread
	Plugins.ShutdownVideoPlugin();
#endif

	if (cpuThread)
	{
		WARN_LOG(CONSOLE, "%s", StopMessage(true, "Stopping CPU thread ...").c_str());
		// There is a CPU thread - join it.
#ifdef _WIN32		
		DWORD Wait = cpuThread->WaitForDeath(3000);
		switch(Wait)
		{
			case WAIT_ABANDONED:
				ERROR_LOG(CONSOLE, "%s", StopMessage(true, "CPU wait returned: WAIT_ABANDONED").c_str());
				break;
			case WAIT_OBJECT_0:
				NOTICE_LOG(CONSOLE, "%s", StopMessage(true, "CPU wait returned: WAIT_OBJECT_0").c_str());
				break;
			case WAIT_TIMEOUT:
				ERROR_LOG(CONSOLE, "%s", StopMessage(true, "CPU wait returned: WAIT_TIMEOUT").c_str());
				break;				
			case WAIT_FAILED:
				ERROR_LOG(CONSOLE, "%s", StopMessage(true, "CPU wait returned: WAIT_FAILED").c_str());
				break;
		}
#else
		cpuThread->WaitForDeath();
#endif
		delete cpuThread;
		// Returns after game exited
		cpuThread = NULL;
	}

	// The hardware is uninitialized
	g_bHwInit = false;
	g_bStopping = false;
	
	NOTICE_LOG(CONSOLE, "%s", StopMessage(true, "Main thread stopped").c_str());
	NOTICE_LOG(CONSOLE, "Stop [Main Thread]\t\t---- Shutdown complete ----");

	Host_UpdateMainFrame();

	return 0;
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
		PanicAlert("Invalid state");
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
	tempname = FULL_SCREENSHOTS_DIR + GetStartupParameter().GetUniqueID();

	name = StringFromFormat("%s-%d.png", tempname.c_str(), index);

	while(File::Exists(name.c_str()))
		name = StringFromFormat("%s-%d.png", tempname.c_str(), ++index);

	return name;
}

void ScreenShot(const std::string& name)
{
	bool bPaused = (GetState() == CORE_PAUSE);

	SetState(CORE_PAUSE);
	CPluginManager::GetInstance().GetVideo()->Video_Screenshot(name.c_str());
	if(!bPaused)
		SetState(CORE_RUN);
}

void ScreenShot()
{
	ScreenShot(GenerateScreenshotName());
}
 
// --- Callbacks for plugins / engine ---
 

// Callback_VideoLog
// WARNING - THIS IS EXECUTED FROM VIDEO THREAD
void Callback_VideoLog(const TCHAR *_szMessage, int _bDoBreak)
{
	INFO_LOG(VIDEO, _szMessage);
}
 
// reports if a frame should be skipped or not
// depending on the framelimit set
bool report_slow(int skipped)
{
	u32 targetfps = SConfig::GetInstance().m_Framelimit * 5;
	double wait_frametime;

	if (targetfps < 5)
		wait_frametime = (1000.0 / VideoInterface::TargetRefreshRate);
	else
		wait_frametime = (1000.0 / targetfps);

	bool fps_slow;

	if (Timer.GetTimeDifference() < wait_frametime * (frames + skipped))
		fps_slow=false;
	else
		fps_slow=true;

	if (targetfps == 5)
		fps_slow=true;

	return fps_slow;
}

// Callback_VideoCopiedToXFB
// WARNING - THIS IS EXECUTED FROM VIDEO THREAD
// We do not write to anything outside this function here
void Callback_VideoCopiedToXFB(bool video_update)
{ 
	if(!video_update)
		Frame::FrameUpdate();

    SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;
	
	//count FPS and VPS
	static u32 videoupd = 0;
	static u32 no_framelimit = 0;
	

	if (video_update)
		videoupd++;
	else
		frames++;

	if (no_framelimit>0)
		no_framelimit--;

	// Custom frame limiter
	// --------------------
	u32 targetfps = SConfig::GetInstance().m_Framelimit * 5;

	if (targetfps > 5)
	{
		double wait_frametime = (1000.0 / targetfps);

		if (Timer.GetTimeDifference() >= wait_frametime * frames)
			no_framelimit = (u32)Timer.GetTimeDifference();

		while (Timer.GetTimeDifference() < wait_frametime * frames)
		{
			if (no_framelimit==0)
				Common::SleepCurrentThread(1);
		}
	}
	else if (targetfps < 5)
	{
		double wait_frametime = (1000.0 / VideoInterface::TargetRefreshRate);

		if (Timer.GetTimeDifference() >= wait_frametime * frames)
			no_framelimit = (u32)Timer.GetTimeDifference();

		while (Timer.GetTimeDifference() < wait_frametime * videoupd)
		{
			// TODO : This is wrong, the sleep shouldn't be there but rather in cputhread
			// as it's not based on the fps but on the refresh rate...
			if (no_framelimit == 0)
				Common::SleepCurrentThread(1);
		}
	}

	if (Timer.GetTimeDifference() >= 1000)
	{
		// Time passed
		float t = (float)(Timer.GetTimeDifference()) / 1000.f;

		// Use extended or summary information. The summary information does not print the ticks data,
		// that's more of a debugging interest, it can always be optional of course if someone is interested.
		//#define EXTENDED_INFO
		#ifdef EXTENDED_INFO
			u64 newTicks = CoreTiming::GetTicks();
			u64 newIdleTicks = CoreTiming::GetIdleTicks();
	 
			s64 diff = (newTicks - ticks) / 1000000;
			s64 idleDiff = (newIdleTicks - idleTicks) / 1000000;
	 
			ticks = newTicks;
			idleTicks = newIdleTicks;	 
			
			float TicksPercentage = (float)diff / (float)(SystemTimers::GetTicksPerSecond() / 1000000) * 100;
		#endif

		float FPS = (float)frames / t;
		// for some reasons "VideoInterface::ActualRefreshRate" gives some odd results :(
		float VPS = (float)videoupd / t;

		int TargetVPS = (int)(VideoInterface::TargetRefreshRate + 0.5);

		float Speed = ((VPS > 0.0f ? VPS : VideoInterface::ActualRefreshRate) / TargetVPS) * 100.0f;
		
		// Settings are shown the same for both extended and summary info
		std::string SSettings = StringFromFormat(" | Core: %s %s",
		#if defined(JITTEST) && JITTEST
			#ifdef _M_IX86
						_CoreParameter.bUseJIT ? "JIT32IL" : "Int32", 
			#else
						_CoreParameter.bUseJIT ? "JIT64IL" : "Int64", 
			#endif
		#else
			#ifdef _M_IX86
						_CoreParameter.bUseJIT ? "JIT32" : "Int32",
			#else
						_CoreParameter.bUseJIT ? "JIT64" : "Int64",
			#endif
		#endif
		_CoreParameter.bUseDualCore ? "DC" : "SC");

		#ifdef EXTENDED_INFO
			std::string SFPS = StringFromFormat("FPS: %4.1f - VPS: %i/%i (%3.0f%%)",
					FPS, VPS > 0 ? (int)VPS : (int)VideoInterface::ActualRefreshRate, TargetVPS, Speed);
			SFPS += StringFromFormat(" | CPU: %s%i MHz [Real: %i + IdleSkip: %i] / %i MHz (%s%3.0f%%)",
					_CoreParameter.bSkipIdle ? "~" : "",
					(int)(diff),
					(int)(diff - idleDiff),
					(int)(idleDiff),
					SystemTimers::GetTicksPerSecond() / 1000000,
					_CoreParameter.bSkipIdle ? "~" : "",
					TicksPercentage);
		
		#else	// Summary information
			std::string SFPS = StringFromFormat("FPS: %4.1f - VPS: %i/%i (%3.0f%%)",
					FPS, VPS > 0 ? (int)VPS : (int)VideoInterface::ActualRefreshRate, TargetVPS, Speed);
		#endif

		// This is our final "frame counter" string
		std::string SMessage = StringFromFormat("%s%s", SFPS.c_str(), SSettings.c_str());

		// Show message
		if (g_pUpdateFPSDisplay != NULL)
			g_pUpdateFPSDisplay(SMessage.c_str()); 

		Host_UpdateStatusBar(SMessage.c_str());

		// Reset frame counter
		frames = 0;
		videoupd = 0;
        Timer.Update();
	}
}


// Callback_DSPLog
// WARNING - THIS MAY BE EXECUTED FROM DSP THREAD
	void Callback_DSPLog(const TCHAR* _szMessage, int _v)
{
	GENERIC_LOG(LogTypes::AUDIO, (LogTypes::LOG_LEVELS)_v, _szMessage);
}
 

// Callback_DSPInterrupt
// WARNING - THIS MAY BE EXECUTED FROM DSP THREAD
void Callback_DSPInterrupt()
{
	DSP::GenerateDSPInterruptFromPlugin(DSP::INT_DSP);
}
 

// Callback_PADLog 
//
void Callback_PADLog(const TCHAR* _szMessage)
{
	// FIXME add levels
	INFO_LOG(SERIALINTERFACE, _szMessage);
}
 

// Callback_ISOName: Let the DSP plugin get the game name
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
// The hotkey function
void Callback_KeyPress(int key, bool shift, bool control)
{
	// F1 - F8: Save states
	if (key >= 0x70 && key < 0x78) {
		// F-key
		int slot_number = key - 0x70 + 1;
		if (shift)
			State_Save(slot_number);
		else
			State_Load(slot_number);
	}

	// 0x78 == VK_F9
	if (key == 0x78)
		ScreenShot();

	// 0x7a == VK_F11
	if (key == 0x7a)
		State_LoadLastSaved();

	// 0x7b == VK_F12
	if (key == 0x7b) 
	{
		if(shift)
			State_UndoSaveState();
		else
			State_UndoLoadState();	
	}
}
 
// Callback_WiimoteLog
//
void Callback_WiimoteLog(const TCHAR* _szMessage, int _v)
{
	GENERIC_LOG(LogTypes::WIIMOTE, (LogTypes::LOG_LEVELS)_v, _szMessage);
}
 
// TODO: Get rid of at some point
const SCoreStartupParameter& GetStartupParameter() {
    return SConfig::GetInstance().m_LocalCoreStartupParameter;
}

} // Core
