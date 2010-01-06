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
bool g_FrameOpen = false;
bool g_SearchDeviceDone = false;
bool g_RealWiiMotePresent = false;
bool g_RealWiiMoteInitialized = false;
bool g_RealWiiMoteAllocated = false;
bool g_EmulatedWiiMoteInitialized = false;

// Settings
accel_cal g_wm;
nu_cal g_nu;
cc_cal g_ClassicContCalibration;
gh3_cal g_GH3Calibration;

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
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);
			if (!wxTheApp || !wxTheApp->CallOnInit())
				return FALSE;
#endif
		}
		break;

	case DLL_PROCESS_DETACH:
#if defined(HAVE_WX) && HAVE_WX
		wxEntryCleanup();
#endif
		break;
	default:
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
	if (!g_SearchDeviceDone)
	{
		// Load settings
		g_Config.Load();
		// We do a pad search before creating the dialog
		WiiMoteEmu::Search_Devices(WiiMoteEmu::joyinfo, WiiMoteEmu::NumPads, WiiMoteEmu::NumGoodPads);
		g_SearchDeviceDone = true;
	}

#if defined(HAVE_WX) && HAVE_WX

	if (!m_BasicConfigFrame)
		m_BasicConfigFrame = new WiimoteBasicConfigDialog(GetParentedWxWindow(_hParent));
	else if (!m_BasicConfigFrame->GetParent()->IsShown())
		m_BasicConfigFrame->Close(true);
	// Update the GUI (because it was not updated when it had been already open before...)
	m_BasicConfigFrame->UpdateGUI();
	// Only allow one open at a time
	if (!m_BasicConfigFrame->IsShown())
	{
		g_FrameOpen = true;
		m_BasicConfigFrame->ShowModal();
	}
	else
	{
		g_FrameOpen = false;
		m_BasicConfigFrame->Hide();
	}

#endif
}

// Start emulation
void Initialize(void *init)
{
	g_EmulatorRunning = true;
	g_WiimoteInitialize =  *(SWiimoteInitialize *)init;

	// Update the GUI if the configuration window is already open
	#if defined(HAVE_WX) && HAVE_WX
	if (g_FrameOpen)
	{
		// Save the settings
		g_Config.Save();
		// Load the ISO Id
		g_ISOId = g_WiimoteInitialize.ISOId;
		// Load the settings
		g_Config.Load();
		if(m_BasicConfigFrame) m_BasicConfigFrame->UpdateGUI();
	}
	#endif
	#if defined(HAVE_X11) && HAVE_X11
		WMdisplay = (Display*)g_WiimoteInitialize.hWnd;
	#endif

	g_ISOId = g_WiimoteInitialize.ISOId;
	DEBUG_LOG(WIIMOTE, "ISOId: %08x %s", g_WiimoteInitialize.ISOId, Hex2Ascii(g_WiimoteInitialize.ISOId).c_str());

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
		//if (g_Config.bConnectRealWiimote) WiiMoteReal::Initialize();
		if(!g_RealWiiMoteInitialized)
			WiiMoteReal::Initialize();
		if(!g_RealWiiMoteAllocated)
			WiiMoteReal::Allocate();
	#endif
}

// If a game is not running this is called by the Configuration window when it's closed
void Shutdown(void)
{
	// Not running
	g_EmulatorRunning = false;

	// Reset the game ID in all cases
	g_ISOId = 0;

	// We will only shutdown when both a game and the m_ConfigFrame is closed
	if (g_FrameOpen)
	{
		#if defined(HAVE_WX) && HAVE_WX
			if(m_BasicConfigFrame) m_BasicConfigFrame->UpdateGUI();
		#endif
	}

#if HAVE_WIIUSE
	if (g_RealWiiMoteInitialized) WiiMoteReal::Shutdown();
#endif
	WiiMoteEmu::Shutdown();
}


void DoState(unsigned char **ptr, int mode)
{
	PointerWrap p(ptr, mode);

	// TODO: Shorten the list

	//p.Do(g_EmulatorRunning);
	//p.Do(g_ISOId);
	//p.Do(g_FrameOpen);
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
	return 1;
}



// Supporting functions

// Check if Dolphin is in focus

bool IsFocus()
{
#ifdef _WIN32
	HWND RenderingWindow = g_WiimoteInitialize.hWnd;
	HWND Parent = GetParent(RenderingWindow);
	HWND TopLevel = GetParent(Parent);
	// Allow updates when the config window is in focus to
	HWND Config = NULL;
	if (m_BasicConfigFrame)
		Config = (HWND)m_BasicConfigFrame->GetHWND();
	// Support both rendering to main window and not
	if (GetForegroundWindow() == TopLevel || GetForegroundWindow() == RenderingWindow || GetForegroundWindow() == Config)
		return true;
	else
		return false;
#else
	return true;
#endif
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

/*
// Turn off all extensions
void DisableExtensions()
{
	g_Config.iExtensionConnected = EXT_NONE;
}

void ReadDebugging(bool Emu, const void* _pData, int Size)
{
	//
	//const u8* data = (const u8*)_pData;
	//u8* data = (u8*)_pData;
	// Copy the data to a new location that we know are the right size
	u8 data[32];
	memset(data, 0, sizeof(data));
	memcpy(data, _pData, Size);

	int size;
	bool DataReport = false;
	std::string Name, TmpData;
	switch(data[1])
	{
	case WM_STATUS_REPORT: // 0x20
		size = sizeof(wm_status_report);
		Name = "WM_STATUS_REPORT";
		{
			wm_status_report* pStatus = (wm_status_report*)(data + 2);
			DEBUG_LOG(WIIMOTE, ""
				"Extension Controller: %i "
				//"Speaker enabled: %i "
				//"IR camera enabled: %i "
				//"LED 1: %i "
				//"LED 2: %i "
				//"LED 3: %i "
				//"LED 4: %i "
				"Battery low: %i",
				pStatus->extension,
				//pStatus->speaker,
				//pStatus->ir,
				//(pStatus->leds >> 0),
				//(pStatus->leds >> 1),
				//(pStatus->leds >> 2),
				//(pStatus->leds >> 3),
				pStatus->battery_low
				);
			// Update the global (for both the real and emulated) extension settings from whatever
			// the real Wiimote use. We will enable the extension from the 0x21 report.
			if(!Emu && !pStatus->extension)
			{
				DisableExtensions();
#if defined(HAVE_WX) && HAVE_WX
				if (m_BasicConfigFrame) m_BasicConfigFrame->UpdateGUI();
#endif
			}
		}
		break;
	case WM_READ_DATA_REPLY: // 0x21
		size = sizeof(wm_read_data_reply);
		Name = "REPLY";
		// data[4]: Size and error
		// data[5, 6]: The registry offset

		// Show the extension ID
		if ((data[4] == 0x10 || data[4] == 0x20 || data[4] == 0x50) && data[5] == 0x00 && (data[6] == 0xfa || data[6] == 0xfe)) 
		{
			if(data[4] == 0x10)
				TmpData.append(StringFromFormat("Game got the encrypted extension ID: %02x%02x", data[7], data[8]));
			else if(data[4] == 0x50)
				TmpData.append(StringFromFormat("Game got the encrypted extension ID: %02x%02x%02x%02x%02x%02x", data[7], data[8], data[9], data[10], data[11], data[12]));

			// We have already sent the data report so we can safely decrypt it now
			if(WiiMoteEmu::g_Encryption)
			{
				if(data[4] == 0x10)
					wiimote_decrypt(&WiiMoteEmu::g_ExtKey, &data[0x07], 0x06, (data[4] >> 0x04) + 1);
				if(data[4] == 0x50)
					wiimote_decrypt(&WiiMoteEmu::g_ExtKey, &data[0x07], 0x02, (data[4] >> 0x04) + 1);
			}

			// Update the global extension settings. Enable the emulated extension from reading
			// what the real Wiimote has connected. To keep the emulated and real Wiimote in sync.
			if(data[4] == 0x10)
			{
				if (!Emu) DisableExtensions();
				if (!Emu && data[7] == 0x00 && data[8] == 0x00)
					g_Config.iExtensionConnected = EXT_NUNCHUCK;
				if (!Emu && data[7] == 0x01 && data[8] == 0x01)
					g_Config.iExtensionConnected = EXT_CLASSIC_CONTROLLER;
				g_Config.Save();
				WiiMoteEmu::UpdateEeprom();
#if defined(HAVE_WX) && HAVE_WX
				if (m_BasicConfigFrame) m_BasicConfigFrame->UpdateGUI();
#endif
				DEBUG_LOG(WIIMOTE, "%s", TmpData.c_str());
				DEBUG_LOG(WIIMOTE, "Game got the decrypted extension ID: %02x%02x", data[7], data[8]);
			}
			else if(data[4] == 0x50)
			{
				if (!Emu) DisableExtensions();
				if (!Emu && data[11] == 0x00 && data[12] == 0x00)
					g_Config.iExtensionConnected = EXT_NUNCHUCK;
				if (!Emu && data[11] == 0x01 && data[12] == 0x01)
					g_Config.iExtensionConnected = EXT_CLASSIC_CONTROLLER;
				g_Config.Save();
				WiiMoteEmu::UpdateEeprom();
#if defined(HAVE_WX) && HAVE_WX
				if (m_BasicConfigFrame) m_BasicConfigFrame->UpdateGUI();
#endif
				DEBUG_LOG(WIIMOTE, "%s", TmpData.c_str());
				DEBUG_LOG(WIIMOTE, "Game got the decrypted extension ID: %02x%02x%02x%02x%02x%02x", data[7], data[8], data[9], data[10], data[11], data[12]);
			}
		}

		// Show the Wiimote neutral values
		// The only difference between the Nunchuck and Wiimote that we go
		// after is calibration here is the offset in memory. If needed we can
		// check the preceding 0x17 request to.
		if(data[4] == 0xf0 && data[5] == 0x00 && data[6] == 0x10)
		{
			if(data[6] == 0x10)
			{
				DEBUG_LOG(WIIMOTE, "Game got the Wiimote calibration:");
				DEBUG_LOG(WIIMOTE, "Cal_zero.x: %i", data[7 + 6]);
				DEBUG_LOG(WIIMOTE, "Cal_zero.y: %i", data[7 + 7]);
				DEBUG_LOG(WIIMOTE, "Cal_zero.z: %i",  data[7 + 8]);
				DEBUG_LOG(WIIMOTE, "Cal_g.x: %i", data[7 + 10]);
				DEBUG_LOG(WIIMOTE, "Cal_g.y: %i",  data[7 + 11]);
				DEBUG_LOG(WIIMOTE, "Cal_g.z: %i",  data[7 +12]);
			}
		}

		// Show the Nunchuck neutral values
		if(data[4] == 0xf0 && data[5] == 0x00 && (data[6] == 0x20 || data[6] == 0x30))
		{
			// Save the encrypted data
			TmpData = StringFromFormat("Read[%s] (enc): %s", (Emu ? "Emu" : "Real"), ArrayToString(data, size + 2, 0, 30).c_str()); 

			// We have already sent the data report so we can safely decrypt it now
			if(WiiMoteEmu::g_Encryption)
				wiimote_decrypt(&WiiMoteEmu::g_ExtKey, &data[0x07], 0x00, (data[4] >> 0x04) + 1);

			if (g_Config.iExtensionConnected == EXT_NUNCHUCK)
			{
				DEBUG_LOG(WIIMOTE, "Game got the Nunchuck calibration:");
				DEBUG_LOG(WIIMOTE, "Cal_zero.x: %i", data[7 + 0]);
				DEBUG_LOG(WIIMOTE, "Cal_zero.y: %i", data[7 + 1]);
				DEBUG_LOG(WIIMOTE, "Cal_zero.z: %i",  data[7 + 2]);
				DEBUG_LOG(WIIMOTE, "Cal_g.x: %i", data[7 + 4]);
				DEBUG_LOG(WIIMOTE, "Cal_g.y: %i",  data[7 + 5]);
				DEBUG_LOG(WIIMOTE, "Cal_g.z: %i",  data[7 + 6]);
				DEBUG_LOG(WIIMOTE, "Js.Max.x: %i",  data[7 + 8]);
				DEBUG_LOG(WIIMOTE, "Js.Min.x: %i",  data[7 + 9]);
				DEBUG_LOG(WIIMOTE, "Js.Center.x: %i", data[7 + 10]);
				DEBUG_LOG(WIIMOTE, "Js.Max.y: %i",  data[7 + 11]);
				DEBUG_LOG(WIIMOTE, "Js.Min.y: %i",  data[7 + 12]);
				DEBUG_LOG(WIIMOTE, "JS.Center.y: %i", data[7 + 13]);
			}
			else // g_Config.bClassicControllerConnected
			{
				DEBUG_LOG(WIIMOTE, "Game got the Classic Controller calibration:");
				DEBUG_LOG(WIIMOTE, "Lx.Max: %i", data[7 + 0]);
				DEBUG_LOG(WIIMOTE, "Lx.Min: %i", data[7 + 1]);
				DEBUG_LOG(WIIMOTE, "Lx.Center: %i",  data[7 + 2]);
				DEBUG_LOG(WIIMOTE, "Ly.Max: %i", data[7 + 3]);
				DEBUG_LOG(WIIMOTE, "Ly.Min: %i",  data[7 + 4]);
				DEBUG_LOG(WIIMOTE, "Ly.Center: %i",  data[7 + 5]);
				DEBUG_LOG(WIIMOTE, "Rx.Max.x: %i",  data[7 + 6]);
				DEBUG_LOG(WIIMOTE, "Rx.Min.x: %i",  data[7 + 7]);
				DEBUG_LOG(WIIMOTE, "Rx.Center.x: %i", data[7 + 8]);
				DEBUG_LOG(WIIMOTE, "Ry.Max.y: %i",  data[7 + 9]);
				DEBUG_LOG(WIIMOTE, "Ry.Min: %i",  data[7 + 10]);
				DEBUG_LOG(WIIMOTE, "Ry.Center: %i", data[7 + 11]);
				DEBUG_LOG(WIIMOTE, "Lt.Neutral: %i",  data[7 + 12]);
				DEBUG_LOG(WIIMOTE, "Rt.Neutral %i", data[7 + 13]);
			}

			// Save the values if they come from the real Wiimote
			if (!Emu)
			{
				// Save the values from the Nunchuck
				if(data[7 + 0] != 0xff)
				{
					memcpy(WiiMoteEmu::g_RegExt + 0x20, &data[7], 0x10);
					memcpy(WiiMoteEmu::g_RegExt + 0x30, &data[7], 0x10);
					
				}
				// Save the default values that should work with Wireless Nunchucks
				else
				{
					WiiMoteEmu::UpdateExtRegisterBlocks();
				}
				WiiMoteEmu::UpdateEeprom();
			}
			// We got a third party nunchuck
			else if(data[7 + 0] == 0xff)
			{
				memcpy(WiiMoteEmu::g_RegExt + 0x20, WiiMoteEmu::wireless_nunchuck_calibration, sizeof(WiiMoteEmu::wireless_nunchuck_calibration));
				memcpy(WiiMoteEmu::g_RegExt + 0x30, WiiMoteEmu::wireless_nunchuck_calibration, sizeof(WiiMoteEmu::wireless_nunchuck_calibration));
			}

			// Show the encrypted data
			DEBUG_LOG(WIIMOTE, "%s", TmpData.c_str());
		}
		
		break;
	case WM_ACK_DATA:  // 0x22
		size = sizeof(wm_acknowledge) - 1;
		Name = "REPLY";
		break;
	case WM_REPORT_CORE: // 0x30-0x37
		size = sizeof(wm_report_core);
		DataReport = true;
		break;
	case WM_REPORT_CORE_ACCEL:
		size = sizeof(wm_report_core_accel);
		DataReport = true;
		break;
	case WM_REPORT_CORE_EXT8:
		size = sizeof(wm_report_core_accel_ir12);
		DataReport = true;
		break;
	case WM_REPORT_CORE_ACCEL_IR12:
		size = sizeof(wm_report_core_accel_ir12);
		DataReport = true;
		break;
	case WM_REPORT_CORE_EXT19:
		size = sizeof(wm_report_core_accel_ext16);
		DataReport = true;
		break;
	case WM_REPORT_CORE_ACCEL_EXT16:
		size = sizeof(wm_report_core_accel_ext16);
		DataReport = true;
		break;
	case WM_REPORT_CORE_IR10_EXT9:
		size = sizeof(wm_report_core_accel_ir10_ext6);
		DataReport = true;
		break;
	case WM_REPORT_CORE_ACCEL_IR10_EXT6:		
		size = sizeof(wm_report_core_accel_ir10_ext6);
		DataReport = true;
		break;
	default:
		//PanicAlert("%s ReadDebugging: Unknown channel 0x%02x", (Emu ? "Emu" : "Real"), data[1]);
		DEBUG_LOG(WIIMOTE, "%s ReadDebugging: Unknown channel 0x%02x", (Emu ? "Emu" : "Real"), data[1]);
		return;
	}

	if (!DataReport && g_DebugComm)
	{
		std::string tmpData = ArrayToString(data, size + 2, 0, 30);
		//LOGV(WIIMOTE, 3, "   Data: %s", Temp.c_str());
		DEBUG_LOG(WIIMOTE, "Read[%s] %s: %s", (Emu ? "Emu" : "Real"), Name.c_str(), tmpData.c_str()); // No timestamp
		//DEBUG_LOG(WIIMOTE, " (%s): %s", Tm(true).c_str(), Temp.c_str()); // Timestamp
	}

	if (DataReport && g_DebugData)
	{
		// Decrypt extension data
		if(WiiMoteEmu::g_ReportingMode == 0x37)
			wiimote_decrypt(&WiiMoteEmu::g_ExtKey, &data[17], 0x00, 0x06);
		if(WiiMoteEmu::g_ReportingMode == 0x35)
			wiimote_decrypt(&WiiMoteEmu::g_ExtKey, &data[7], 0x00, 0x06);

		// Produce string
		//std::string TmpData = ArrayToString(data, size + 2, 0, 30);
		//LOGV(WIIMOTE, 3, "   Data: %s", Temp.c_str());
		std::string TmpCore = "", TmpAccel = "", TmpIR = "", TmpExt = "", CCData = "";
		TmpCore = StringFromFormat(
				"%02x %02x %02x %02x",
				data[0], data[1], data[2], data[3]);  // Header and core buttons

		TmpAccel = StringFromFormat(
				"%03i %03i %03i",
				data[4], data[5], data[6]); // Wiimote accelerometer

		if (data[1] == 0x33) // WM_REPORT_CORE_ACCEL_IR12
		{
			TmpIR = StringFromFormat(
				"%02x %02x %02x %02x %02x %02x"
				" %02x %02x %02x %02x %02x %02x",
				data[7], data[8], data[9], data[10], data[11], data[12],
				data[13], data[14], data[15], data[16], data[17], data[18]);
		}
		if (data[1] == 0x35) // WM_REPORT_CORE_ACCEL_EXT16
		{
			TmpExt = StringFromFormat(
				"%02x %02x %02x %02x %02x %02x",
			data[7], data[8], // Nunchuck stick
			data[9], data[10], data[11], // Nunchuck Accelerometer
			data[12]); //  Nunchuck buttons

			CCData = WiiMoteEmu::CCData2Values(&data[7]);
		}
		if (data[1] == 0x37) // WM_REPORT_CORE_ACCEL_IR10_EXT6
		{
			TmpIR = StringFromFormat(
				"%02x %02x %02x %02x %02x"
				" %02x %02x %02x %02x %02x",
				data[7], data[8], data[9], data[10], data[11],
				data[12], data[13], data[14], data[15], data[16]);
			TmpExt = StringFromFormat(
				"%02x %02x %02x %02x %02x %02x",	
				data[17], data[18], // Nunchuck stick
				data[19], data[20], data[21], // Nunchuck Accelerometer
				data[22]); //  Nunchuck buttons
			CCData = WiiMoteEmu::CCData2Values(&data[17]);
		}


		// ---------------------------------------------
		// Calculate the Wiimote roll and pitch in degrees
		// -----------
		int Roll, Pitch, RollAdj, PitchAdj;
		WiiMoteEmu::PitchAccelerometerToDegree(data[4], data[5], data[6], Roll, Pitch, RollAdj, PitchAdj);
		std::string RollPitch = StringFromFormat("%s %s  %s %s",
			(Roll >= 0) ? StringFromFormat(" %03i", Roll).c_str() : StringFromFormat("%04i", Roll).c_str(),
			(Pitch >= 0) ? StringFromFormat(" %03i", Pitch).c_str() : StringFromFormat("%04i", Pitch).c_str(),
			(RollAdj == Roll) ? "     " : StringFromFormat("%04i*", RollAdj).c_str(),
			(PitchAdj == Pitch) ? "     " : StringFromFormat("%04i*", PitchAdj).c_str());
		// -------------------------

		// ---------------------------------------------
		// Test the angles to x, y, z values formula by calculating the values back and forth
		// -----------
		ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
		Console->ClearScreen();
		// Show a test of our calculations
		WiiMoteEmu::TiltTest(data[4], data[5], data[6]);
		u8 x, y, z;
		//WiiMoteEmu::Tilt(x, y, z);
		//WiiMoteEmu::TiltTest(x, y, z);	

		// -------------------------

		// ---------------------------------------------
		// Show the number of g forces on the axes
		// -----------
		float Gx = WiiMoteEmu::AccelerometerToG((float)data[4], (float)g_wm.cal_zero.x, (float)g_wm.cal_g.x);
		float Gy = WiiMoteEmu::AccelerometerToG((float)data[5], (float)g_wm.cal_zero.y, (float)g_wm.cal_g.y);
		float Gz = WiiMoteEmu::AccelerometerToG((float)data[6], (float)g_wm.cal_zero.z, (float)g_wm.cal_g.z);
		std::string GForce = StringFromFormat("%s %s %s",
			((int)Gx >= 0) ? StringFromFormat(" %i", (int)Gx).c_str() : StringFromFormat("%i", (int)Gx).c_str(),
			((int)Gy >= 0) ? StringFromFormat(" %i", (int)Gy).c_str() : StringFromFormat("%i", (int)Gy).c_str(),
			((int)Gz >= 0) ? StringFromFormat(" %i", (int)Gz).c_str() : StringFromFormat("%i", (int)Gz).c_str());
		// -------------------------

		// ---------------------------------------------
		// Calculate the IR data
		// -----------
		if (data[1] == WM_REPORT_CORE_ACCEL_IR10_EXT6) WiiMoteEmu::IRData2DotsBasic(&data[7]); else WiiMoteEmu::IRData2Dots(&data[7]);
		std::string IRData;
		// Create a shortcut
		struct WiiMoteEmu::SDot* Dot = WiiMoteEmu::g_Wiimote_kbd.IR.Dot;
		for (int i = 0; i < 4; ++i)
		{
			if(Dot[i].Visible)
				IRData += StringFromFormat("[%i] X:%04i Y:%04i Size:%i ", Dot[i].Order, Dot[i].Rx, Dot[i].Ry, Dot[i].Size);
			else
				IRData += StringFromFormat("[%i]", Dot[i].Order);
		}
		// Dot distance
		IRData += StringFromFormat(" | Distance:%i", WiiMoteEmu::g_Wiimote_kbd.IR.Distance);
		// -------------------------

		// Classic Controller data
		DEBUG_LOG(WIIMOTE, "Read[%s]: %s | %s | %s | %s | %s", (Emu ? "Emu" : "Real"),
			TmpCore.c_str(), TmpAccel.c_str(), TmpIR.c_str(), TmpExt.c_str(), CCData.c_str());
		// Formatted data only
		//DEBUG_LOG(WIIMOTE, "Read[%s]: 0x%02x | %s | %s | %s", (Emu ? "Emu" : "Real"), data[1], RollPitch.c_str(), GForce.c_str(), IRData.c_str());
		// IR data
		//DEBUG_LOG(WIIMOTE, "Read[%s]: %s | %s", (Emu ? "Emu" : "Real"), TmpData.c_str(), IRData.c_str());
		// Accelerometer data
		//DEBUG_LOG(WIIMOTE, "Read[%s]: %s | %s | %s | %s | %s | %s | %s", (Emu ? "Emu" : "Real"),
		//	TmpCore.c_str(), TmpAccel.c_str(), TmpIR.c_str(), TmpExt.c_str(), RollPitch.c_str(), GForce.c_str(), CCData.c_str());
		// Timestamp
		//DEBUG_LOG(WIIMOTE, " (%s): %s", Tm(true).c_str(), Temp.c_str());
		
	}
	if(g_DebugAccelerometer)
	{		
		// Accelerometer only
		//		Console::ClearScreen();	
		DEBUG_LOG(WIIMOTE, "Accel x, y, z: %03u %03u %03u", data[4], data[5], data[6]);
	}
}

void InterruptDebugging(bool Emu, const void* _pData)
{
	const u8* data = (const u8*)_pData;
	
	std::string Name;
	int size;
	u16 SampleValue;
	bool SoundData = false;

	if (g_DebugComm) Name += StringFromFormat("Write[%s] ", (Emu ? "Emu" : "Real"));
	
	switch(data[1])
	{
	case 0x10:
		size = 4; // I don't know the size
		if (g_DebugComm) Name.append("0x10");
		break;
	case WM_LEDS: // 0x11
		size = sizeof(wm_leds);
		if (g_DebugComm) Name.append("WM_LEDS");
		break;
	case WM_REPORT_MODE: // 0x12
		size = sizeof(wm_report_mode);
		if (g_DebugComm) Name.append("WM_REPORT_MODE");
		break;
	case WM_REQUEST_STATUS: // 0x15
		size = sizeof(wm_request_status);
		if (g_DebugComm) Name.append("WM_REQUEST_STATUS");
		break;
	case WM_WRITE_DATA: // 0x16
		if (g_DebugComm) Name.append("WM_WRITE_DATA");
		size = sizeof(wm_write_data);
		// data[2]: The address space 0, 1 or 2
		// data[3]: The registry type
		// data[5]: The registry offset
		// data[6]: The number of bytes
		switch(data[2] >> 0x01)
		{
		case WM_SPACE_EEPROM: 
			if (g_DebugComm) Name.append(" REG_EEPROM"); break;
		case WM_SPACE_REGS1:
		case WM_SPACE_REGS2:
			switch(data[3])
			{
			case 0xa2:
				// data[8]: FF, 0x00 or 0x40
				// data[9, 10]: RR RR, 0xd007 or 0x401f
				// data[11]: VV, 0x00 to 0xff or 0x00 to 0x40
				if (g_DebugComm)
				{
					Name.append(" REG_SPEAKER");
					if(data[6] == 7)
					{
						DEBUG_LOG(WIIMOTE, "Sound configuration:");
						if(data[8] == 0x00)
						{
							memcpy(&SampleValue, &data[9], 2);
							DEBUG_LOG(WIIMOTE, "    Data format: 4-bit ADPCM (%i Hz)", 6000000 / SampleValue);
							DEBUG_LOG(WIIMOTE, "    Volume: %02i%%", (data[11] / 0x40) * 100);
						}
						else if (data[8] == 0x40)
						{
							memcpy(&SampleValue, &data[9], 2);
							DEBUG_LOG(WIIMOTE, "    Data format: 8-bit PCM (%i Hz)", 12000000 / SampleValue);
							DEBUG_LOG(WIIMOTE, "    Volume: %02i%%", (data[11] / 0xff) * 100);
						}
					}
				}
				break;
			case 0xa4:
				if (g_DebugComm) Name.append(" REG_EXT");
				// Update the encryption mode
				if (data[3] == 0xa4 && data[5] == 0xf0)
				{
					if (data[7] == 0xaa)
						WiiMoteEmu::g_Encryption = true;
					else if (data[7] == 0x55)
						WiiMoteEmu::g_Encryption = false;
					DEBUG_LOG(WIIMOTE, "Extension enryption turned %s", WiiMoteEmu::g_Encryption ? "On" : "Off");
				}		
				break;
			case 0xb0:
				 if (g_DebugComm) Name.append(" REG_IR"); break;
			}
			break;
		}
		break;
	case WM_READ_DATA: // 0x17
		size = sizeof(wm_read_data);
		// data[2]: The address space 0, 1 or 2
		// data[3]: The registry type
		// data[5]: The registry offset
		// data[7]: The number of bytes, 6 and 7 together
		if (g_DebugComm) Name.append("WM_READ_DATA");
		switch(data[2] >> 0x01)
		{
		case WM_SPACE_EEPROM:
			if (g_DebugComm) Name.append(" REG_EEPROM"); break;
		case WM_SPACE_REGS1:
		case WM_SPACE_REGS2:
			switch(data[3])
			{
			case 0xa2:
				if (g_DebugComm) Name.append(" REG_SPEAKER"); break;
			case 0xa4:
				 if (g_DebugComm) Name.append(" REG_EXT"); break;
			case 0xb0:
				if (g_DebugComm) Name.append(" REG_IR"); break;
			}
			break;
		}
		break;

	case WM_IR_PIXEL_CLOCK: // 0x13
	case WM_IR_LOGIC: // 0x1a
		if (g_DebugComm) Name.append("WM_IR");
		size = 1;
		break;
	case WM_SPEAKER_ENABLE: // 0x14
	case WM_SPEAKER_MUTE: // 0x19
		if (g_DebugComm) Name.append("WM_SPEAKER");
		size = 1;
		if(data[1] == 0x14) {
			DEBUG_LOG(WIIMOTE, "Speaker %s", (data[2] & 4) ? "On" : "Off");
		} else if(data[1] == 0x19) {
			DEBUG_LOG(WIIMOTE, "Speaker %s", (data[2] & 4) ? "Muted" : "Unmuted");
		}
		break;
	case WM_WRITE_SPEAKER_DATA: // 0x18
		if (g_DebugComm) Name.append("WM_SPEAKER_DATA");
		size = 21;
		break;

	default:
		size = 15;
		DEBUG_LOG(WIIMOTE, "%s InterruptDebugging: Unknown channel 0x%02x", (Emu ? "Emu" : "Real"), data[1]);
		break;
	}
	if (g_DebugComm && !SoundData)
	{
		std::string Temp = ArrayToString(data, size + 2, 0, 30);
		//LOGV(WIIMOTE, 3, "   Data: %s", Temp.c_str());
		DEBUG_LOG(WIIMOTE, "%s: %s", Name.c_str(), Temp.c_str());
		//DEBUG_LOG(WIIMOTE, " (%s): %s", Tm(true).c_str(), Temp.c_str());
	}
	if (g_DebugSoundData && SoundData)
	{
		std::string Temp = ArrayToString(data, size + 2, 0, 30);
		//LOGV(WIIMOTE, 3, "   Data: %s", Temp.c_str());
		DEBUG_LOG(WIIMOTE, "%s: %s", Name.c_str(), Temp.c_str());
		//DEBUG_LOG(WIIMOTE, " (%s): %s", Tm(true).c_str(), Temp.c_str());
	}
	
}
*/
