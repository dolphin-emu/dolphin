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
#include "Config.h"
#include "StringUtil.h"
#include "ConsoleWindow.h" // For Start, Print, GetHwnd

#if defined(HAVE_WX) && HAVE_WX
	#include <wx/aboutdlg.h>
	#include "ConfigDlg.h"
#endif

#define EXCLUDEMAIN_H // Avoid certain declarations in main.h
#include "main.h" // Local
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

bool g_EmulatorRunning = false;
bool g_FrameOpen = false;
bool g_RealWiiMotePresent = false;
bool g_RealWiiMoteInitialized = false;
bool g_EmulatedWiiMoteInitialized = false;

HINSTANCE g_hInstance;

#if defined(HAVE_WX) && HAVE_WX
	wxWindow win;
	ConfigDialog *frame;

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

	g_FrameOpen = true;	
	frame = new ConfigDialog(&win);

	DoInitialize();

	frame->ShowModal();
	//frame.Show();	

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

		// Don't shut down the wiimote when we still have the window open
		return;
	}

#if HAVE_WIIUSE
	if(g_RealWiiMoteInitialized) WiiMoteReal::Shutdown();
#endif
	WiiMoteEmu::Shutdown();

	Console::Close();
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
	}

	if (!g_RealWiiMotePresent)
		WiiMoteEmu::ControlChannel(_channelID, _pData, _Size);
#if HAVE_WIIUSE
	else
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
	if (!g_Config.bUseRealWiimote || !g_RealWiiMotePresent)
		WiiMoteEmu::Update();
#if HAVE_WIIUSE
	else if (g_RealWiiMotePresent)
		WiiMoteReal::Update();
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

void DoInitialize()
{
	// ----------------------------------------
	// Debugging window
	// ----------
	/*Console::Open(100, 750, "Wiimote"); // give room for 20 rows
	Console::Print("Wiimote console opened\n");

	// Move window
	//MoveWindow(Console::GetHwnd(), 0,400, 100*8,10*14, true); // small window
	MoveWindow(Console::GetHwnd(), 400,0, 100*8,70*14, true); // big window*/
	// ---------------

	// Load config settings
	g_Config.Load();

	/* We will run WiiMoteReal::Initialize() even if we are not using a real wiimote,
	   to check if there is a real wiimote connected. We will initiate wiiuse.dll, but
	   we will return before creating a new thread for it if we find no real Wiimotes.
	   Then g_RealWiiMotePresent will also be false. This function call will be done
	   instantly if there is no real Wiimote connected.  I'm not sure how long time
	   it takes if a Wiimote is connected. */
	#if HAVE_WIIUSE
		if (g_Config.bConnectRealWiimote) WiiMoteReal::Initialize();
	#endif

	WiiMoteEmu::Initialize();
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



