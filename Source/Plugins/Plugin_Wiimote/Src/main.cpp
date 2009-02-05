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
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
#include "Common.h" // Common
#include "StringUtil.h"
#include "ConsoleWindow.h" // For Start, Print, GetHwnd
#include "Timer.h"

#define EXCLUDEMAIN_H // Avoid certain declarations in main.h
#include "EmuDefinitions.h"  // Local
#include "wiimote_hid.h"
#include "main.h"
#include "Logging.h"
#if defined(HAVE_WX) && HAVE_WX
	#include "ConfigDlg.h"
#endif
#include "Config.h"
#include "pluginspecs_wiimote.h"
#include "EmuMain.h"
#if HAVE_WIIUSE
	#include "wiimote_real.h"
#endif
///////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
SWiimoteInitialize g_WiimoteInitialize;

// General
bool g_EmulatorRunning = false;
bool g_FrameOpen = false;
bool g_RealWiiMotePresent = false;
bool g_RealWiiMoteInitialized = false;
bool g_EmulatedWiiMoteInitialized = false;

// Settings
accel_cal g_accel;

// Debugging
bool g_DebugAccelerometer = false;
bool g_DebugData = false;
bool g_DebugComm = true;

// Update speed
int g_UpdateCounter = 0;
double g_UpdateTime = 0;
int g_UpdateRate = 0;
int g_UpdateWriteScreen = 0;
std::vector<int> g_UpdateTimeList (5, 0);

// Movement recording
std::vector<SRecordingAll> VRecording(RECORDING_ROWS); 

// DLL instance
HINSTANCE g_hInstance;

#if defined(HAVE_WX) && HAVE_WX
	wxWindow win;
	ConfigDialog *frame = NULL;

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
////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Main function and WxWidgets initialization
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
BOOL APIENTRY DllMain(HINSTANCE hinstDLL,	// DLL module handle
					  DWORD dwReason,		// reason called
					  LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{       //use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);
			if ( !wxTheApp || !wxTheApp->CallOnInit() )
				return FALSE;
		}
		break;

	case DLL_PROCESS_DETACH:
		wxEntryCleanup(); //use wxUninitialize() if you don't want GUI
		break;
	default:
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}
#endif
/////////////////////////////////////


//******************************************************************************
// Exports
//******************************************************************************
extern "C" void GetDllInfo (PLUGIN_INFO* _PluginInfo)
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

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals) {}

void DllDebugger(HWND _hParent, bool Show) {}

void DllConfig(HWND _hParent)
{
#if defined(HAVE_WX) && HAVE_WX

	#ifdef _WIN32
		win.SetHWND(_hParent);
	#endif

	//Console::Open();
	DoInitialize();

	frame = new ConfigDialog(&win);
	g_FrameOpen = true;

	frame->ShowModal();	

	#ifdef _WIN32
		win.SetHWND(0);
	#endif

#endif
}

extern "C" void Initialize(void *init)
{
	// Declarations
    SWiimoteInitialize _WiimoteInitialize = *(SWiimoteInitialize *)init;
	g_WiimoteInitialize = _WiimoteInitialize;

	g_EmulatorRunning = true;

	#if defined(HAVE_WX) && HAVE_WX
		if(g_FrameOpen) if(frame) frame->UpdateGUI();
	#endif

	DoInitialize();	
}

// If a game is not running this is called by the Configuration window when it's closed
extern "C" void Shutdown(void)
{
	// Not running
	g_EmulatorRunning = false;

	// We will only shutdown when both a game and the frame is closed
	if (g_FrameOpen)
	{
		#if defined(HAVE_WX) && HAVE_WX
			if(frame) frame->UpdateGUI();
		#endif

		/* Don't shut down the wiimote when we still have the config window open, we may still want
		   want to use the Wiimote in the config window. */
		return;
	}

#if HAVE_WIIUSE
	if(g_RealWiiMoteInitialized) WiiMoteReal::Shutdown();
#endif
	WiiMoteEmu::Shutdown();

	//Console::Close();
}


extern "C" void DoState(unsigned char **ptr, int mode)
{
#if HAVE_WIIUSE
	WiiMoteReal::DoState(ptr, mode);
#endif
	WiiMoteEmu::DoState(ptr, mode);
}


// ===================================================
/* This function produce Wiimote Input (reports from the Wiimote) in response
   to Output from the Wii. It's called from WII_IPC_HLE_WiiMote.cpp.
   
   Switch between real and emulated wiimote: We send all this Input to WiiMoteEmu::InterruptChannel()
   so that it knows the channel ID and the data reporting mode at all times.
   */
// ----------------
extern "C" void Wiimote_InterruptChannel(u16 _channelID, const void* _pData, u32 _Size)
{
	LOGV(WII_IPC_WIIMOTE, 3, "=============================================================");
	const u8* data = (const u8*)_pData;

	// Debugging
	{
		LOGV(WII_IPC_WIIMOTE, 3, "Wiimote_Input");
		LOGV(WII_IPC_WIIMOTE, 3, "   Channel ID: %04x", _channelID);
		std::string Temp = ArrayToString(data, _Size);
		LOGV(WII_IPC_WIIMOTE, 3, "   Data: %s", Temp.c_str());
	}

	// Decice where to send the message
	//if (!g_RealWiiMotePresent)
		WiiMoteEmu::InterruptChannel(_channelID, _pData, _Size);		
#if HAVE_WIIUSE
	if (g_RealWiiMotePresent)
		WiiMoteReal::InterruptChannel(_channelID, _pData, _Size);
#endif
		
	LOGV(WII_IPC_WIIMOTE, 3, "=============================================================");
}
// ==============================


// ===================================================
/* Function: Used for the initial Bluetooth HID handshake. */
// ----------------
extern "C" void Wiimote_ControlChannel(u16 _channelID, const void* _pData, u32 _Size)
{
	LOGV(WII_IPC_WIIMOTE, 3, "=============================================================");
	const u8* data = (const u8*)_pData;

	// Debugging
	{
		LOGV(WII_IPC_WIIMOTE, 3, "Wiimote_ControlChannel");
		std::string Temp = ArrayToString(data, _Size);
		LOGV(WII_IPC_WIIMOTE, 3, "    Data: %s", Temp.c_str());
		//PanicAlert("Wiimote_ControlChannel");
	}

	//if (!g_RealWiiMotePresent)
		WiiMoteEmu::ControlChannel(_channelID, _pData, _Size);
#if HAVE_WIIUSE
	if (g_RealWiiMotePresent)
		WiiMoteReal::ControlChannel(_channelID, _pData, _Size);
#endif
		
	LOGV(WII_IPC_WIIMOTE, 3, "=============================================================");
}
// ==============================


// ===================================================
/* This sends a Data Report from the Wiimote. See SystemTimers.cpp for the documentation of this
   update. */
// ----------------
extern "C" void Wiimote_Update()
{
	// Tell us about the update rate, but only about once every second to avoid a major slowdown
	if (frame)
	{
		GetUpdateRate();
		if (g_UpdateWriteScreen > g_UpdateRate)
		{
			frame->m_TextUpdateRate->SetLabel(wxString::Format(wxT("Update rate: %03i times/s"), g_UpdateRate));
			g_UpdateWriteScreen = 0;
		}
		g_UpdateWriteScreen++;
	}

	// This functions will send:
	//		Emulated Wiimote: Only data reports 0x30-0x37
	//		Real Wiimote: Both data reports 0x30-0x37 and all other read reports
	if (!g_Config.bUseRealWiimote || !g_RealWiiMotePresent)
		WiiMoteEmu::Update();
#if HAVE_WIIUSE
	else if (g_RealWiiMotePresent)
		WiiMoteReal::Update();
#endif

	// Debugging
#ifdef _WIN32
	if( GetAsyncKeyState(VK_HOME) && g_DebugComm ) g_DebugComm = false;
		else if (GetAsyncKeyState(VK_HOME) && !g_DebugComm ) g_DebugComm = true;

	if( GetAsyncKeyState(VK_PRIOR) && g_DebugData ) g_DebugData = false;
		else if (GetAsyncKeyState(VK_PRIOR) && !g_DebugData ) g_DebugData = true;

	if( GetAsyncKeyState(VK_NEXT) && g_DebugAccelerometer ) g_DebugAccelerometer = false;
		else if (GetAsyncKeyState(VK_NEXT) && !g_DebugAccelerometer ) g_DebugAccelerometer = true;
#endif
}

extern "C" unsigned int Wiimote_GetAttachedControllers()
{
	return 1;
}
// ================


//******************************************************************************
// Supporting functions
//******************************************************************************

// Check if Dolphin is in focus
bool IsFocus()
{
#ifdef _WIN32
	HWND Parent = GetParent(g_WiimoteInitialize.hWnd);
	HWND TopLevel = GetParent(Parent);
	// Support both rendering to main window and not
	if (GetForegroundWindow() == TopLevel || GetForegroundWindow() == g_WiimoteInitialize.hWnd)
		return true;
	else
		return false;
#else
	return true;
#endif
}

void ReadDebugging(bool Emu, const void* _pData, int Size)
{
	//
	//const u8* data = (const u8*)_pData;
	u8* data = (u8*)_pData;

	int size;
	bool DataReport = false;
	std::string Name;
	switch(data[1])
	{
	case WM_STATUS_REPORT: // 0x20
		size = sizeof(wm_status_report);
		Name = "WM_STATUS_REPORT";
		{
			/*wm_status_report* pStatus = (wm_status_report*)(data + 2);
			Console::Print(""
				"Extension Controller: %i\n"
				"Speaker enabled: %i\n"
				"IR camera enabled: %i\n"
				"LED 1: %i\n"
				"LED 2: %i\n"
				"LED 3: %i\n"
				"LED 4: %i\n"
				"Battery low: %i\n\n",
				pStatus->extension,
				pStatus->speaker,
				pStatus->ir,
				(pStatus->leds >> 0),
				(pStatus->leds >> 1),
				(pStatus->leds >> 2),
				(pStatus->leds >> 3),
				pStatus->battery_low
				);*/
		}
		break;
	case WM_READ_DATA_REPLY: // 0x21
		size = sizeof(wm_read_data_reply);
		Name = "REPLY";
		// data[4]: Size and error
		// data[5, 6]: The registry offset

		// Show the accelerometer neutral values
		if (data[5] == 0x00 && data[6] == 0x10)
		{
			Console::Print("\nGame got the Wiimote accelerometer neutral values: %i %i %i\n\n",
				data[13], data[14], data[19]);
		}
		// Show the nunchuck neutral values
		// We have already sent the data report so we can safely decrypt it now
		wiimote_decrypt(&WiiMoteEmu::g_ExtKey, &data[7], 0x00, Size - 7);
		if(data[4] == 0xf0 && data[5] == 0x00 && data[6] == 0x20)
		{
			Console::Print("\nGame got the Nunchuck calibration:\n");
			Console::Print("Cal_zero.x: %i\n", data[7 + 0]);
			Console::Print("Cal_zero.y: %i\n", data[7 + 1]);
			Console::Print("Cal_zero.z: %i\n",  data[7 + 2]);
			Console::Print("Cal_g.x: %i\n", data[7 + 4]);
			Console::Print("Cal_g.y: %i\n",  data[7 + 5]);
			Console::Print("Cal_g.z: %i\n",  data[7 + 6]);
			Console::Print("Js.Max.x: %i\n",  data[7 + 8]);
			Console::Print("Js.Min.x: %i\n",  data[7 + 9]);
			Console::Print("Js.Center.x: %i\n", data[7 + 10]);
			Console::Print("Js.Max.y: %i\n",  data[7 + 11]);
			Console::Print("Js.Min.y: %i\n",  data[7 + 12]);
			Console::Print("JS.Center.y: %i\n\n", data[7 + 13]);
		}
		
		break;
	case WM_WRITE_DATA_REPLY:  // 0x22
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
		PanicAlert("%s ReadDebugging: Unknown channel 0x%02x", (Emu ? "Emu" : "Real"), data[1]);
		return;
	}

	if (!DataReport && g_DebugComm)
	{
		std::string TmpData = ArrayToString(data, size + 2, 0, 30);
		//LOGV(WII_IPC_WIIMOTE, 3, "   Data: %s", Temp.c_str());
		Console::Print("Read[%s] %s: %s\n", (Emu ? "Emu" : "Real"), Name.c_str(), TmpData.c_str()); // No timestamp
		//Console::Print(" (%s): %s\n", Tm(true).c_str(), Temp.c_str()); // Timestamp
	}

	if (DataReport && g_DebugData)
	{
		// Decrypt extension data
		if(WiiMoteEmu::g_ReportingMode == 0x37)
			wiimote_decrypt(&WiiMoteEmu::g_ExtKey, &data[17], 0x00, 0x06);

		// Produce string
		std::string TmpData = ArrayToString(data, size + 2, 0, 30);
		//LOGV(WII_IPC_WIIMOTE, 3, "   Data: %s", Temp.c_str());

		// Format accelerometer values to regular 10 base values
		if(TmpData.length() > 20)
		{
			std::string Tmp1 = TmpData.substr(0, 12);
			std::string Tmp2 = TmpData.substr(20, (TmpData.length() - 20));
			TmpData = Tmp1 + StringFromFormat("%03i %03i %03i", data[4], data[5], data[6]) + Tmp2;
		}
		// Format accelerometer values for the Nunchuck to regular 10 base values
		if(TmpData.length() > 68 && WiiMoteEmu::g_ReportingMode == 0x37)
		{
			std::string Tmp1 = TmpData.substr(0, 60);
			std::string Tmp2 = TmpData.substr(68, (TmpData.length() - 68));
			TmpData = Tmp1 + StringFromFormat("%03i %03i %03i", data[19], data[20], data[21]) + Tmp2;
		}

		Console::Print("Read[%s]: %s\n", (Emu ? "Emu" : "Real"), TmpData.c_str()); // No timestamp
		//Console::Print(" (%s): %s\n", Tm(true).c_str(), Temp.c_str()); // Timestamp
	}
	if(g_DebugAccelerometer)
	{		
		// Accelerometer only
		Console::ClearScreen();	
		Console::Print("Accel x, y, z: %03u %03u %03u\n", data[4], data[5], data[6]);
	}
}


void InterruptDebugging(bool Emu, const void* _pData)
{
	//
	const u8* data = (const u8*)_pData;
	
	if (g_DebugComm) Console::Print("Write[%s] ", (Emu ? "Emu" : "Real"));

	int size;
	switch(data[1])
	{
	case 0x10:
		size = 4; // I don't know the size
		if (g_DebugComm) Console::Print("0x10");
		break;
	case WM_LEDS: // 0x11
		size = sizeof(wm_leds);
		if (g_DebugComm) Console::Print("WM_LEDS");
		break;
	case WM_DATA_REPORTING: // 0x12
		size = sizeof(wm_data_reporting);
		if (g_DebugComm) Console::Print("WM_DATA_REPORTING");
		break;
	case WM_REQUEST_STATUS: // 0x15
		size = sizeof(wm_request_status);
		if (g_DebugComm) Console::Print("WM_REQUEST_STATUS");
		break;
	case WM_WRITE_DATA: // 0x16
		if (g_DebugComm) Console::Print("WM_WRITE_DATA");
		size = sizeof(wm_write_data);
		// data[2]: The address space 0, 1 or 2
		// data[3]: The registry type
		// data[5]: The registry offset
		// data[6]: The number of bytes
		switch(data[2] >> 0x01)
		{
		case WM_SPACE_EEPROM: 
			if (g_DebugComm) Console::Print(" REG_EEPROM"); break;
		case WM_SPACE_REGS1:
		case WM_SPACE_REGS2:
			switch(data[3])
			{
			case 0xa2:
				if (g_DebugComm) Console::Print(" REG_SPEAKER"); break;
			case 0xa4:
				if (g_DebugComm) Console::Print(" REG_EXT"); break;
			case 0xb0:
				 if (g_DebugComm) Console::Print(" REG_IR"); break;
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
		if (g_DebugComm) Console::Print("WM_READ_DATA");
		switch(data[2] >> 0x01)
		{
		case WM_SPACE_EEPROM:
			if (g_DebugComm) Console::Print(" REG_EEPROM"); break;
		case WM_SPACE_REGS1:
		case WM_SPACE_REGS2:
			switch(data[3])
			{
			case 0xa2:
				if (g_DebugComm) Console::Print(" REG_SPEAKER"); break;
			case 0xa4:
				 if (g_DebugComm) Console::Print(" REG_EXT"); break;
			case 0xb0:
				if (g_DebugComm) Console::Print(" REG_IR"); break;
			}
			break;
		}
		break;
	case WM_IR_PIXEL_CLOCK: // 0x13
	case WM_IR_LOGIC: // 0x1a
		if (g_DebugComm) Console::Print("WM_IR");
		size = 1;
		break;
	case WM_SPEAKER_ENABLE: // 0x14
	case WM_SPEAKER_MUTE:
		if (g_DebugComm) Console::Print("WM_SPEAKER");
		size = 1;
		break;
	default:
		size = 15;
		Console::Print("%s InterruptDebugging: Unknown channel 0x%02x", (Emu ? "Emu" : "Real"), data[1]);
		break;
	}
	if (g_DebugComm)
	{
		std::string Temp = ArrayToString(data, size + 2, 0, 30);
		//LOGV(WII_IPC_WIIMOTE, 3, "   Data: %s", Temp.c_str());
		Console::Print(": %s\n",  Temp.c_str()); // No timestamp
		//Console::Print(" (%s): %s\n", Tm(true).c_str(), Temp.c_str()); // Timestamp
	}
}


/* Returns a timestamp with three decimals for precise time comparisons. The return format is
   of the form seconds.milleseconds for example 1234.123. The leding seconds have no particular meaning
   but are just there to enable use to tell if we have entered a new second or now. */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
double GetDoubleTime()
{
#if defined(HAVE_WX) && HAVE_WX
	wxDateTime datetime = wxDateTime::UNow(); // Get timestamp
	u64 TmpSeconds = Common::Timer::GetTimeSinceJan1970(); // Get continous timestamp

	/* Remove a few years. We only really want enough seconds to make sure that we are
	   detecting actual actions, perhaps 60 seconds is enough really, but I leave a
	   year of seconds anyway, in case the user's clock is incorrect or something like that */
	TmpSeconds = TmpSeconds - (38 * 365 * 24 * 60 * 60);

	//if (TmpSeconds < 0) return 0; // Check the the user's clock is working somewhat

	u32 Seconds = (u32)TmpSeconds; // Make a smaller integer that fits in the double
	double ms = datetime.GetMillisecond() / 1000.0;
	double TmpTime = Seconds + ms;
	return TmpTime;
#endif
}

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
		int Time = (int)(10 / (GetDoubleTime() - g_UpdateTime));
		g_UpdateTimeList.push_back(Time);
		//Console::Print("Time: %i %f\n", Time, GetDoubleTime());

		int TotalTime = 0;
		for (int i = 0; i < g_UpdateTimeList.size(); i++)
			TotalTime += g_UpdateTimeList.at(i);
		g_UpdateRate = TotalTime / 5;

		// Write the new update time
		g_UpdateTime = GetDoubleTime();

		g_UpdateCounter = 0;
	}

	g_UpdateCounter++;

	return g_UpdateRate;
#else
	return 0;
#endif
}

void DoInitialize()
{
	// ----------------------------------------
	// Debugging window
	// ----------
	/*Console::Open(130, 1000, "Wiimote"); // give room for 20 rows
	Console::Print("\n\n\nWiimote console opened\n");

	// Move window
	//MoveWindow(Console::GetHwnd(), 0,400, 100*8,10*14, true); // small window
	//MoveWindow(Console::GetHwnd(), 400,0, 100*8,70*14, true); // big window
	MoveWindow(Console::GetHwnd(), 200,0, 130*8,70*14, true); // big wide window*/
	// ---------------

	// Load config settings
	g_Config.Load();

	// Run this first so that WiiMoteReal::Initialize() overwrites g_Eeprom
	WiiMoteEmu::Initialize();

	/* We will run WiiMoteReal::Initialize() even if we are not using a real wiimote,
	   to check if there is a real wiimote connected. We will initiate wiiuse.dll, but
	   we will return before creating a new thread for it if we find no real Wiimotes.
	   Then g_RealWiiMotePresent will also be false. This function call will be done
	   instantly if there is no real Wiimote connected.  I'm not sure how long time
	   it takes if a Wiimote is connected. */
	#if HAVE_WIIUSE
		if (g_Config.bConnectRealWiimote) WiiMoteReal::Initialize();
	#endif
}


//******************************************************************************
// Logging functions
//******************************************************************************

void __Log(int log, const char *_fmt, ...)
{
	char Msg[512];
	va_list ap;

	va_start( ap, _fmt );
	vsprintf( Msg, _fmt, ap );
	va_end( ap );

	g_WiimoteInitialize.pLog(Msg, 0);
}


void __Logv(int log, int v, const char *_fmt, ...)
{
	char Msg[512];
	va_list ap;

	va_start( ap, _fmt );
	vsprintf( Msg, _fmt, ap );
	va_end( ap );

	g_WiimoteInitialize.pLog(Msg, v);
}



