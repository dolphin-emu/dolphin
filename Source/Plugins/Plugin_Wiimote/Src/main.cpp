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


#include "Common.h" // Common
#include "LogManager.h"
#include "StringUtil.h"
#include "Timer.h"

#define EXCLUDEMAIN_H // Avoid certain declarations in main.h
#include "EmuDefinitions.h"  // Local
#include "wiimote_hid.h"
#include "main.h"
#if defined(HAVE_WX) && HAVE_WX
	#include "ConfigPadDlg.h"
	#include "ConfigRecordingDlg.h"
	#include "ConfigBasicDlg.h"

	WiimotePadConfigDialog *m_PadConfigFrame = NULL;
	WiimoteRecordingConfigDialog *m_RecordingConfigFrame = NULL;
	WiimoteBasicConfigDialog *m_BasicConfigFrame = NULL;
#endif
#include "Config.h"
#include "pluginspecs_wiimote.h"
#include "EmuMain.h"
#if HAVE_WIIUSE
	#include "wiimote_real.h"
#endif

#if defined(HAVE_X11) && HAVE_X11
	Display* WMdisplay;
#endif
SWiimoteInitialize g_WiimoteInitialize;
PLUGIN_GLOBALS* globals = NULL;

// General
bool g_EmulatorRunning = false;
u32 g_ISOId = 0;
bool g_SearchDeviceDone = false;
bool g_RealWiiMotePresent = false;

// Debugging
bool g_DebugAccelerometer = false;
bool g_DebugData = false;
bool g_DebugComm = false;
bool g_DebugSoundData = false;
bool g_DebugCustom = false;

// Update speed
int g_UpdateCounter = 0;
double g_UpdateTime = 0;
int g_UpdateRate = 0;
int g_UpdateWriteScreen = 0;
std::vector<int> g_UpdateTimeList (5, 0);

// Movement recording
std::vector<SRecordingAll> VRecording(RECORDING_ROWS);

PLUGIN_EMUSTATE g_EmulatorState = PLUGIN_EMUSTATE_STOP;

// Standard crap to make wxWidgets happy
#ifdef _WIN32
HINSTANCE g_hInstance;

#if defined(HAVE_WX) && HAVE_WX
class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp)
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

BOOL APIENTRY DllMain(HINSTANCE hinstDLL,	// DLL module handle
					  DWORD dwReason,		// reason called
					  LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
#if defined(HAVE_WX) && HAVE_WX
			wxSetInstance((HINSTANCE)hinstDLL);
			wxInitialize();
#endif
		}
		break;

	case DLL_PROCESS_DETACH:
		{
#ifdef _WIN32
			if (g_Config.bUnpairRealWiimote){
				WiiMoteReal::Shutdown();
				WiiMoteReal::WiimotePairUp(true);
			}
#endif
#if defined(HAVE_WX) && HAVE_WX
		wxUninitialize();
#endif
		}
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}
#endif

#if defined(HAVE_WX) && HAVE_WX
wxWindow* GetParentedWxWindow(HWND Parent)
{
#ifdef _WIN32
	wxSetInstance((HINSTANCE)g_hInstance);
#endif
	wxWindow *win = new wxWindow();
#ifdef _WIN32
	win->SetHWND((WXHWND)Parent);
	win->AdoptAttributesFromHWND();
#endif
	return win;
}
#endif

// Exports
void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_WIIMOTE;
#ifdef DEBUGFAST
	sprintf(_PluginInfo->Name, "Dolphin Wiimote Plugin (DebugFast)");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin Wiimote Plugin");
#else
	sprintf(_PluginInfo->Name, "Dolphin Wiimote Plugin (Debug)");
#endif
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	 globals = _pPluginGlobals;
	 LogManager::SetInstance((LogManager *)globals->logManager);
}

void DllDebugger(HWND _hParent, bool Show) {}

void DllConfig(HWND _hParent)
{
#ifdef _WIN32
	if (WiiMoteReal::g_AutoPairUpInvisibleWindow == NULL)
	{
		WiiMoteReal::g_AutoPairUpInvisibleWindow = new Common::Thread(WiiMoteReal::RunInvisibleMessageWindow_ThreadFunc, NULL);
		WiiMoteReal::g_AutoPairUpMonitoring = new Common::Thread(WiiMoteReal::PairUp_ThreadFunc, NULL);
	}
#endif
	if (!g_SearchDeviceDone)
	{
		// Load settings
		g_Config.Load();
		// We do a pad search before creating the dialog
		WiiMoteEmu::Search_Devices(WiiMoteEmu::joyinfo, WiiMoteEmu::NumPads, WiiMoteEmu::NumGoodPads);
		g_SearchDeviceDone = true;
	}


#if defined(HAVE_WX) && HAVE_WX
	wxWindow *frame = GetParentedWxWindow(_hParent);
	m_BasicConfigFrame = new WiimoteBasicConfigDialog(frame);
#ifdef _WIN32
	frame->Disable();
	m_BasicConfigFrame->ShowModal();
	frame->Enable();
#else
	m_BasicConfigFrame->ShowModal();
#endif

#ifdef _WIN32
	frame->SetFocus();
	frame->SetHWND(NULL);
#endif

	m_BasicConfigFrame->Destroy();
	m_BasicConfigFrame = NULL;
	frame->Destroy();
#endif
}

// Start emulation
void Initialize(void *init)
{
	g_EmulatorRunning = true;
	g_WiimoteInitialize =  *(SWiimoteInitialize *)init;

	#if defined(HAVE_WX) && HAVE_WX
	// Load the ISO Id
	g_ISOId = g_WiimoteInitialize.ISOId;
	// Load the settings
	g_Config.Load();
	#endif
	#if defined(HAVE_X11) && HAVE_X11
		WMdisplay = (Display*)g_WiimoteInitialize.hWnd;
	#endif

	g_ISOId = g_WiimoteInitialize.ISOId;
	DEBUG_LOG(WIIMOTE, "ISOId: %08x %s", g_WiimoteInitialize.ISOId, Hex2Ascii(g_WiimoteInitialize.ISOId).c_str());

	// Load IR settings, as this is a per-game setting and the user might have loaded a different game
	g_Config.LoadIR();

	// Run this first so that WiiMoteReal::Initialize() overwrites g_Eeprom
	WiiMoteEmu::Initialize();

	/* We will run WiiMoteReal::Initialize() even if we are not using a real
	   wiimote, to check if there is a real wiimote connected. We will initiate
	   wiiuse.dll, but we will return before creating a new thread for it if we
	   find no real Wiimotes.  Then g_RealWiiMotePresent will also be
	   false. This function call will be done instantly whether there is a real
	   Wiimote connected or not. It takes no time for Wiiuse to check for
	   connected Wiimotes. */
#if HAVE_WIIUSE

	WiiMoteReal::Initialize();
	WiiMoteReal::Allocate();
#ifdef _WIN32
	if (WiiMoteReal::g_AutoPairUpInvisibleWindow == NULL)
	{
		WiiMoteReal::g_AutoPairUpInvisibleWindow = new Common::Thread(WiiMoteReal::RunInvisibleMessageWindow_ThreadFunc, NULL);
		WiiMoteReal::g_AutoPairUpMonitoring = new Common::Thread(WiiMoteReal::PairUp_ThreadFunc, NULL);
	}
#endif
#endif
}

// If a game is not running this is called by the Configuration window when it's closed
void Shutdown(void)
{
	// Not running
	g_EmulatorRunning = false;

	// Reset the game ID in all cases
	g_ISOId = 0;

#if HAVE_WIIUSE
	WiiMoteReal::Shutdown();
#endif
	WiiMoteEmu::Shutdown();
}


void DoState(unsigned char **ptr, int mode)
{
	PointerWrap p(ptr, mode);

	// TODO: Shorten the list

	//p.Do(g_EmulatorRunning);
	//p.Do(g_ISOId);
	//p.Do(g_RealWiiMotePresent);
	//p.Do(g_RealWiiMoteInitialized);
	//p.Do(g_EmulatedWiiMoteInitialized);
	//p.Do(g_UpdateCounter);
	//p.Do(g_UpdateTime);
	//p.Do(g_UpdateRate);
	//p.Do(g_UpdateWriteScreen);
	//p.Do(g_UpdateTimeList);

#if HAVE_WIIUSE
	WiiMoteReal::DoState(p);
#endif
	WiiMoteEmu::DoState(p);

	return;
}
void EmuStateChange(PLUGIN_EMUSTATE newState)
{
	g_EmulatorState = newState;
}

// Hack to use wx key events
volatile bool wxkeystate[256];

// Set buttons status from keyboard input. Currently this is done from
// wxWidgets in the main application.   
// --------------
void Wiimote_Input(u16 _Key, u8 _UpDown)
{
#if defined(__APPLE__) && defined(USE_WX) && USE_WX
        if (_Key < 256)
        {
                wxkeystate[_Key] = _UpDown;
        }
#endif
}

/* This function produce Wiimote Input (reports from the Wiimote) in response
   to Output from the Wii. It's called from WII_IPC_HLE_WiiMote.cpp.
   
   Switch between real and emulated wiimote: We send all this Input to WiiMoteEmu::InterruptChannel()
   so that it knows the channel ID and the data reporting mode at all times.
   */
void Wiimote_InterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	// Debugging
#if defined(_DEBUG) || defined(DEBUGFAST)
	DEBUG_LOG(WIIMOTE, "Wiimote_InterruptChannel");
	DEBUG_LOG(WIIMOTE, "    Channel ID: %04x", _channelID);
	std::string Temp = ArrayToString((const u8*)_pData, _Size);
	DEBUG_LOG(WIIMOTE, "    Data: %s", Temp.c_str());
#endif

	// Decice where to send the message
	if (WiiMoteEmu::WiiMapping[_number].Source <= 1)
		WiiMoteEmu::InterruptChannel(_number, _channelID, _pData, _Size);
#if HAVE_WIIUSE
	else if (g_RealWiiMotePresent)
		WiiMoteReal::InterruptChannel(_number, _channelID, _pData, _Size);
#endif
}


// Function: Used for the initial Bluetooth HID handshake.
void Wiimote_ControlChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	// Debugging
#if defined(_DEBUG) || defined(DEBUGFAST)
	DEBUG_LOG(WIIMOTE, "Wiimote_ControlChannel");
	DEBUG_LOG(WIIMOTE, "    Channel ID: %04x", _channelID);
	std::string Temp = ArrayToString((const u8*)_pData, _Size);
	DEBUG_LOG(WIIMOTE, "    Data: %s", Temp.c_str());
#endif

	// Check for custom communication
	if(_channelID == 99 && *(const u8*)_pData == WIIMOTE_DISCONNECT)
	{
		WiiMoteEmu::g_ReportingAuto[_number] = false;
		WARN_LOG(WIIMOTE, "Wiimote: #%i Disconnected", _number);
#ifdef _WIN32
		PostMessage(g_WiimoteInitialize.hWnd, WM_USER, WIIMOTE_DISCONNECT, _number);
#endif
		return;
	}

	if (WiiMoteEmu::WiiMapping[_number].Source <= 1)
		WiiMoteEmu::ControlChannel(_number, _channelID, _pData, _Size);
#if HAVE_WIIUSE
	else if (g_RealWiiMotePresent)
		WiiMoteReal::ControlChannel(_number, _channelID, _pData, _Size);
#endif
}


// This sends a Data Report from the Wiimote. See SystemTimers.cpp for the documentation of this update.
void Wiimote_Update(int _number)
{
	// Tell us about the update rate, but only about once every second to avoid a major slowdown
#if defined(HAVE_WX) && HAVE_WX
	if (m_RecordingConfigFrame)
	{
		GetUpdateRate();
		if (g_UpdateWriteScreen > g_UpdateRate)
		{
			m_RecordingConfigFrame->m_TextUpdateRate->SetLabel(wxString::Format(wxT("Update rate: %03i times/s"), g_UpdateRate));
			g_UpdateWriteScreen = 0;
		}
		g_UpdateWriteScreen++;
	}
#endif

	// This functions will send:
	//		Emulated Wiimote: Only data reports 0x30-0x37
	//		Real Wiimote: Both data reports 0x30-0x37 and all other read reports
	if (WiiMoteEmu::WiiMapping[_number].Source <= 1)
		WiiMoteEmu::Update(_number);
#if HAVE_WIIUSE
	else if (g_RealWiiMotePresent)
		WiiMoteReal::Update(_number);
#endif

/*
	// Debugging
#ifdef _WIN32
	if( GetAsyncKeyState(VK_HOME) && g_DebugComm ) g_DebugComm = false; // Page Down
		else if (GetAsyncKeyState(VK_HOME) && !g_DebugComm ) g_DebugComm = true;

	if( GetAsyncKeyState(VK_PRIOR) && g_DebugData ) g_DebugData = false; // Page Up
		else if (GetAsyncKeyState(VK_PRIOR) && !g_DebugData ) g_DebugData = true;

	if( GetAsyncKeyState(VK_NEXT) && g_DebugAccelerometer ) g_DebugAccelerometer = false; // Home
		else if (GetAsyncKeyState(VK_NEXT) && !g_DebugAccelerometer ) g_DebugAccelerometer = true;

	if( GetAsyncKeyState(VK_END) && g_DebugCustom ) { g_DebugCustom = false; DEBUG_LOG(WIIMOTE, "Custom Debug: Off");} // End
		else if (GetAsyncKeyState(VK_END) && !g_DebugCustom ) {g_DebugCustom = true; DEBUG_LOG(WIIMOTE, "Custom Debug: Off");}
#endif
*/

}

unsigned int Wiimote_GetAttachedControllers()
{
	unsigned int attached = 0;
	for (unsigned int i=0; i<4; ++i)
		if (WiiMoteEmu::WiiMapping[i].Source)
			attached |= (1 << i);
	return attached;
}



// Supporting functions

// Check if the render window is in focus

bool IsFocus()
{
#if defined(__APPLE__) && defined(USE_WX) && USE_WX
	return true;	/* XXX */
#endif
	return g_WiimoteInitialize.pRendererHasFocus();
}

/* Returns a timestamp with three decimals for precise time comparisons. The return format is
   of the form seconds.milleseconds for example 1234.123. The leding seconds have no particular meaning
   but are just there to enable use to tell if we have entered a new second or now. */
// -----------------

/* Calculate the current update frequency. Calculate the time between ten updates, and average
   five such rates. If we assume there are 60 updates per second if the game is running at full
   speed then we get this measure on average once every second. The reason to have a few updates
   between each measurement is becase the milliseconds may not be perfectly accurate and may return
   the same time even when a milliseconds has actually passed, for example.*/
int GetUpdateRate()
{
#if defined(HAVE_WX) && HAVE_WX
	if(g_UpdateCounter == 10)
	{
		// Erase the old ones
		if(g_UpdateTimeList.size() == 5) g_UpdateTimeList.erase(g_UpdateTimeList.begin() + 0);

		// Calculate the time and save it
		int Time = (int)(10 / (Common::Timer::GetDoubleTime() - g_UpdateTime));
		g_UpdateTimeList.push_back(Time);
		//DEBUG_LOG(WIIMOTE, "Time: %i %f", Time, Common::Timer::GetDoubleTime());

		int TotalTime = 0;
		for (int i = 0; i < (int)g_UpdateTimeList.size(); i++)
			TotalTime += g_UpdateTimeList.at(i);
		g_UpdateRate = TotalTime / 5;

		// Write the new update time
		g_UpdateTime = Common::Timer::GetDoubleTime();

		g_UpdateCounter = 0;
	}

	g_UpdateCounter++;

	return g_UpdateRate;
#else
	return 0;
#endif
}
