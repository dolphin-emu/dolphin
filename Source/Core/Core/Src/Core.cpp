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
 
 
//////////////////////////////////////////////////////////////////////////////////////////
// Include
// 
#ifdef _WIN32
	#include <windows.h>
#else
#endif

#include "Thread.h" // Common
#include "Timer.h"
#include "Common.h"
#include "ConsoleWindow.h"

#include "Console.h"
#include "Core.h"
#include "CPUDetect.h"
#include "CoreTiming.h"
#include "Boot/Boot.h"
#include "PatchEngine.h"
 
#include "HW/Memmap.h"
#include "HW/PeripheralInterface.h"
#include "HW/GPFifo.h"
#include "HW/CPU.h"
#include "HW/CPUCompare.h"
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
 
#ifndef _WIN32
#define WINAPI
#endif
////////////////////////////////////////
 
 
// The idea behind the recent restructure is to fix various stupid problems.
// glXMakeCurrent/ wglMakeCurrent takes a context and makes it current on the current thread.
// So it's fine to init ogl on one thread, and then make it current and start blasting on another.
 
 
namespace Core
{
 
//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// 
 
// Function forwarding
//void Callback_VideoRequestWindowSize(int _iWidth, int _iHeight, BOOL _bFullscreen);
void Callback_VideoLog(const TCHAR* _szMessage, int _bDoBreak);
void Callback_VideoCopiedToXFB();
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
 
bool g_bHwInit = false;
bool g_bRealWiimote = false;
HWND g_pWindowHandle = NULL;
Common::Thread* g_pThread = NULL;
SCoreStartupParameter g_CoreStartupParameter; 
Common::Event emuThreadGoing;
//////////////////////////////////////
 
 
//////////////////////////////////////////////////////////////////////////////////////////
// Display messages and return values
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
	CCPU::EnableStepping(true);
}
 
void* GetWindowHandle()
{
    return g_pWindowHandle;
}
 
bool GetRealWiimote()
{
    return g_bRealWiimote;
}
/////////////////////////////////////
 
 
//////////////////////////////////////////////////////////////////////////////////////////
// This is called from the GUI thread. See the booting call schedule in BootManager.cpp
// -----------------
bool Init()
{
	//Console::Open();

	if (g_pThread != NULL)
	{
		PanicAlert("ERROR: Emu Thread already running. Report this bug.");
		return false;
	}
 
	CPluginManager &pManager  = CPluginManager::GetInstance();
	SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	g_CoreStartupParameter = _CoreParameter;
	LogManager::Init();	
	Host_SetWaitCursor(true);
 
	// Start the thread again
	_dbg_assert_(HLE, g_pThread == NULL);
 
	// LoadLibrary()
	if (!pManager.InitPlugins())
	    return false;

	emuThreadGoing.Init();
	// This will execute EmuThread() further down in this file
	g_pThread = new Common::Thread(EmuThread, NULL);
	emuThreadGoing.Wait();
	emuThreadGoing.Shutdown();
	// all right ... here we go
	Host_SetWaitCursor(false);
	DisplayMessage("CPU: " + cpu_info.Summarize(), 8000);
	DisplayMessage(_CoreParameter.m_strFilename, 3000);
 
	//PluginVideo::DllDebugger(NULL);
 
	return true;
}
 
 
// Called from GUI thread or VI thread
void Stop() // - Hammertime!
{
	Host_SetWaitCursor(true);
	if (PowerPC::state == PowerPC::CPU_POWERDOWN)
		return;
 
        // stop the CPU
        PowerPC::state = PowerPC::CPU_POWERDOWN;
 
        CCPU::StepOpcode(); //kick it if it's waiting
 
	// The quit is to get it out of its message loop
	// Should be moved inside the plugin.
	#ifdef _WIN32
		PostMessage((HWND)g_pWindowHandle, WM_QUIT, 0, 0);
	#else
		CPluginManager::GetInstance().GetVideo()->Video_Stop();
	#endif
 
	#ifdef _WIN32
		/* I have to use this to avoid the hangings, it seems harmless and it works so I'm
		   okay with it */
		if (GetParent((HWND)g_pWindowHandle) == NULL)
			delete g_pThread; // Wait for emuthread to close
	#else
		delete g_pThread;
	#endif
	g_pThread = 0;
	Core::StopTrace();
	LogManager::Shutdown();
	Host_SetWaitCursor(false);
}
 
 
//////////////////////////////////////////////////////////////////////////////////////////
// Create the CPU thread
// ---------------
THREAD_RETURN CpuThread(void *pArg)
{
    Common::SetCurrentThreadName("CPU thread");
 
    const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

    if (!_CoreParameter.bUseDualCore)
	{
	    //wglMakeCurrent
	    CPluginManager::GetInstance().GetVideo()->Video_Prepare(); 
	}
 
    if (_CoreParameter.bRunCompareServer)
	{
	    CPUCompare::StartServer();
	    PowerPC::state = PowerPC::CPU_RUNNING;
	}
    else if (_CoreParameter.bRunCompareClient)
	{
	    PanicAlert("Compare Debug : Press OK when ready.");
	    CPUCompare::ConnectAsClient();
	}
 
    if (_CoreParameter.bLockThreads)
	Common::Thread::SetCurrentThreadAffinity(1); //Force to first core
 
    if (_CoreParameter.bUseFastMem)
	{
		#ifdef _M_X64
			// Let's run under memory watch
			EMM::InstallExceptionHandler();
		#else
			PanicAlert("32-bit platforms do not support fastmem yet. Report this bug.");
		#endif
	}
 
    CCPU::Run();
 
    if (_CoreParameter.bRunCompareServer || _CoreParameter.bRunCompareClient)
	{
	    CPUCompare::Stop();
	}
    return 0;
}
//////////////////////////////////////////
 
 
//////////////////////////////////////////////////////////////////////////////////////////
// Initalize plugins and create emulation thread
// -------------
	/* Call browser: Init():g_pThread(). See the BootManager.cpp file description for a complete
		call schedule. */
THREAD_RETURN EmuThread(void *pArg)
{
	Common::SetCurrentThreadName("Emuthread - starting");
	const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;

	CPluginManager &Plugins = CPluginManager::GetInstance();
	if (_CoreParameter.bLockThreads)
		Common::Thread::SetCurrentThreadAffinity(2); // Force to second core
 
	LOG(OSREPORT, "Starting core = %s mode", _CoreParameter.bWii ? "Wii" : "Gamecube");
	LOG(OSREPORT, "Dualcore = %s", _CoreParameter.bUseDualCore ? "Yes" : "No");

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
	VideoInitialize.pVIRegs             = VideoInterface::m_UVIUnknownRegs;
	VideoInitialize.pPeekMessages       = NULL;
	VideoInitialize.pUpdateFPSDisplay   = NULL;
	VideoInitialize.pCPFifo             = (SCPFifoStruct*)&CommandProcessor::fifo;
	VideoInitialize.pUpdateInterrupts   = &(CommandProcessor::UpdateInterruptsFromVideoPlugin);
	VideoInitialize.pMemoryBase         = Memory::base;
	VideoInitialize.pKeyPress           = Callback_KeyPress;
	VideoInitialize.bWii                = _CoreParameter.bWii;
	VideoInitialize.bUseDualCore		= _CoreParameter.bUseDualCore;
	Plugins.FreeVideo(); // This is needed for Stop and Start
	Plugins.GetVideo()->Initialize(&VideoInitialize); // Call the dll
 
	// Under linux, this is an X11 Display, not an HWND!
	g_pWindowHandle = (HWND)VideoInitialize.pWindowHandle;
	Callback_PeekMessages = VideoInitialize.pPeekMessages;
	g_pUpdateFPSDisplay = VideoInitialize.pUpdateFPSDisplay;

    // Load and init DSPPlugin	
	DSPInitialize dspInit;
	dspInit.hWnd = g_pWindowHandle;
	dspInit.pARAM_Read_U8 = (u8  (__cdecl *)(const u32))DSP::ReadARAM; 
	dspInit.pGetARAMPointer = DSP::GetARAMPtr;
	dspInit.pGetMemoryPointer = Memory::GetPointer;
	dspInit.pLog = Callback_DSPLog;
	dspInit.pName = Callback_ISOName;
	dspInit.pDebuggerBreak = Callback_DebuggerBreak;
	dspInit.pGenerateDSPInterrupt = Callback_DSPInterrupt;
	dspInit.pGetAudioStreaming = AudioInterface::Callback_GetStreaming;
	dspInit.pEmulatorState = (int *)&PowerPC::state;
	Plugins.GetDSP()->Initialize((void *)&dspInit);

	// Load and Init PadPlugin
	for (int i = 0; i < MAXPADS; i++)
	{			
		SPADInitialize PADInitialize;
		PADInitialize.hWnd = g_pWindowHandle;
		PADInitialize.pLog = Callback_PADLog;
		PADInitialize.padNumber = i;
		// Check if we should init the plugin
		if(Plugins.OkayToInitPlugin(i))
		{
			Plugins.GetPad(i)->Initialize(&PADInitialize);

			// Check if joypad open failed, in that case try again
			if(PADInitialize.padNumber == -1)
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
		WiimoteInitialize.pLog = Callback_WiimoteLog;
		WiimoteInitialize.pWiimoteInput = Callback_WiimoteInput;
		// Wait for Wiiuse to find the number of connected Wiimotes
		Plugins.GetWiimote(0)->Initialize((void *)&WiimoteInitialize);
	}
 
	// The hardware is initialized.
	g_bHwInit = true;
 
	// Load GCM/DOL/ELF whatever ... we boot with the interpreter core
	PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
	CBoot::BootUp();
 
	if( g_pUpdateFPSDisplay != NULL )
        g_pUpdateFPSDisplay("Loading...");
 
	// setup our core, but can't use dynarec if we are compare server
	if (_CoreParameter.bUseJIT && (!_CoreParameter.bRunCompareServer || _CoreParameter.bRunCompareClient))
		PowerPC::SetMode(PowerPC::MODE_JIT);
	else
		PowerPC::SetMode(PowerPC::MODE_INTERPRETER);
 
	// update the window again because all stuff is initialized
	Host_UpdateDisasmDialog();
	Host_UpdateMainFrame();
 
	//This thread, after creating the EmuWindow, spawns a CPU thread,
	//then takes over and becomes the graphics thread
 
	//In single core mode, the CPU thread does the graphics. In fact, the
	//CPU thread should in this case also create the emuwindow...
 
	// Spawn the CPU thread
	Common::Thread *cpuThread = NULL;
 
	//////////////////////////////////////////////////////////////////////////
	// ENTER THE VIDEO THREAD LOOP
	//////////////////////////////////////////////////////////////////////////
 
	if (!_CoreParameter.bUseDualCore)
	{
		#ifdef _WIN32
			cpuThread = new Common::Thread(CpuThread, pArg);
			//Common::SetCurrentThreadName("Idle thread");
			//TODO(ector) : investigate using GetMessage instead .. although
			//then we lose the powerdown check. ... unless powerdown sends a message :P
			while (PowerPC::state != PowerPC::CPU_POWERDOWN)
			{
				if (Callback_PeekMessages) Callback_PeekMessages();
				Common::SleepCurrentThread(20);
			}
		#else
			// In single-core mode, the Emulation main thread is also the CPU thread
			CpuThread(pArg);
		#endif
	}
	else
	{
	    Plugins.GetVideo()->Video_Prepare(); // wglMakeCurrent
	    cpuThread = new Common::Thread(CpuThread, pArg);
	    Common::SetCurrentThreadName("Video thread");
	    Plugins.GetVideo()->Video_EnterLoop();
	}
 
	// Wait for CPU thread to exit - it should have been signaled to do so by now
	if (cpuThread)
	    cpuThread->WaitForDeath();
	if (g_pUpdateFPSDisplay != NULL)
	    g_pUpdateFPSDisplay("Stopping...");
 
	if (cpuThread)
	{
		delete cpuThread;  // This joins the cpu thread.
		// Returns after game exited
		cpuThread = NULL;
	}

	// The hardware is uninitialized
	g_bHwInit = false;

	Plugins.ShutdownPlugins();
 
	HW::Shutdown();
 
	LOG(MASTER_LOG, "EmuThread exited");
	// The CPU should return when a game is stopped and cleanup should be done here, 
	// so we can restart the plugins (or load new ones) for the next game.
	if (_CoreParameter.hMainWindow == g_pWindowHandle)
		Host_UpdateMainFrame();

	//Console::Close();
	return 0;
}
 
 
//////////////////////////////////////////////////////////////////////////////////////////
// Set or get the running state
// --------------
bool SetState(EState _State)
{
	switch(_State)
	{
	case CORE_UNINITIALIZED:
        Stop();
		break;
 
	case CORE_PAUSE:
		CCPU::EnableStepping(true);
		break;
 
	case CORE_RUN:
		CCPU::EnableStepping(false);
		break;
 
	default:
		return false;
	}
 
	return true;
}
 
EState GetState()
{
	if (g_bHwInit)
	{
		if (CCPU::IsStepping())
			return CORE_PAUSE;
		else
			return CORE_RUN;
	}
	return CORE_UNINITIALIZED;
}
///////////////////////////////////////
 
 
//////////////////////////////////////////////////////////////////////////////////////////
// Save or recreate the emulation state
// ----------
void SaveState() {
    State_Save(0);
}
 
void LoadState() {
    State_Load(0);
}
///////////////////////////////////////
 
 
bool MakeScreenshot(const std::string &filename)
{
 
	bool bResult = false;
	CPluginManager &pManager  = CPluginManager::GetInstance();
 
	if (pManager.GetVideo()->IsValid())
	{
		TCHAR szTmpFilename[MAX_PATH];
		strcpy(szTmpFilename, filename.c_str());
		bResult = pManager.GetVideo()->Video_Screenshot(szTmpFilename) ? true : false;
	}
	return bResult;
}
 
 
/////////////////////////////////////////////////////////////////////////////////////////////////////
// --- Callbacks for plugins / engine ---
/////////////////////////////////////////////////////////////////////////////////////////////////////
 
// __________________________________________________________________________________________________
// Callback_VideoLog
// WARNING - THIS IS EXECUTED FROM VIDEO THREAD
void Callback_VideoLog(const TCHAR *_szMessage, int _bDoBreak)
{
	LOG(VIDEO, _szMessage);
}
 
// __________________________________________________________________________________________________
// Callback_VideoCopiedToXFB
// WARNING - THIS IS EXECUTED FROM VIDEO THREAD
// We do not touch anything outside this function here
void Callback_VideoCopiedToXFB()
{ 
    SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;
	//count FPS
	static Common::Timer Timer;
	static u32 frames = 0;
	static u64 ticks = 0;
	static u64 idleTicks = 0;

	frames++;
 
	if (Timer.GetTimeDifference() >= 1000)
	{
		u64 newTicks = CoreTiming::GetTicks();
		u64 newIdleTicks = CoreTiming::GetIdleTicks();
 
		s64 diff = (newTicks - ticks)/1000000;
		s64 idleDiff = (newIdleTicks - idleTicks)/1000000;
 
		ticks = newTicks;
		idleTicks = newIdleTicks;
 
		float t = (float)(Timer.GetTimeDifference()) / 1000.f;
		char temp[256];
		sprintf(temp, "FPS:%8.2f - Core: %s | %s - Speed: %i MHz [Real: %i + IdleSkip: %i] / %i MHz", 
			(float)frames / t, 
			_CoreParameter.bUseJIT ? "JIT" : "Interpreter", 
			_CoreParameter.bUseDualCore ? "DC" : "SC",
			(int)(diff),
			(int)(diff-idleDiff),
			(int)(idleDiff),
			SystemTimers::GetTicksPerSecond()/1000000);
 
		if (g_pUpdateFPSDisplay != NULL)
		    g_pUpdateFPSDisplay(temp);
 
		Host_UpdateStatusBar(temp);
 
 
		frames = 0;
        Timer.Update();
	}
	PatchEngine::ApplyFramePatches();
	PatchEngine::ApplyARPatches();
}
 
// __________________________________________________________________________________________________
// Callback_DSPLog
// WARNING - THIS MAY EXECUTED FROM DSP THREAD
void Callback_DSPLog(const TCHAR* _szMessage, int _v)
{
	LOGV(AUDIO, _v, _szMessage);
}
 
// __________________________________________________________________________________________________
// Callback_DSPInterrupt
// WARNING - THIS MAY EXECUTED FROM DSP THREAD
void Callback_DSPInterrupt()
{
	DSP::GenerateDSPInterruptFromPlugin(DSP::INT_DSP);
}
 
// __________________________________________________________________________________________________
// Callback_PADLog
//
void Callback_PADLog(const TCHAR* _szMessage)
{
	LOG(SERIALINTERFACE, _szMessage);
}
 
// __________________________________________________________________________________________________
// Callback_ISOName: Let the DSP plugin get the game name
//
//std::string Callback_ISOName(void)
const char *Callback_ISOName(void)
{
    SCoreStartupParameter& params = SConfig::GetInstance().m_LocalCoreStartupParameter;
	if (params.m_strName.length() > 0)
		return (const char *)params.m_strName.c_str();
	else	
          return (const char *)"";
}
 
// __________________________________________________________________________________________________
// Called from ANY thread!
void Callback_KeyPress(int key, bool shift, bool control)
{
	// 0x70 == VK_F1
	if (key >= 0x70 && key < 0x79) {
		// F-key
		int slot_number = key - 0x70 + 1;
		if (shift) {
			State_Save(slot_number);
		} else {
			State_Load(slot_number);
		}
	}
}
 
// __________________________________________________________________________________________________
// Callback_WiimoteLog
//
void Callback_WiimoteLog(const TCHAR* _szMessage, int _v)
{
	LOGV(WII_IPC_WIIMOTE, _v, _szMessage);
}
 
// TODO: Get rid of at some point
const SCoreStartupParameter& GetStartupParameter() {
    return SConfig::GetInstance().m_LocalCoreStartupParameter;
}

} // end of namespace Core

