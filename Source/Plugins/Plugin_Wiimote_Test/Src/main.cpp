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


#if !defined(OSX64)
#include <wx/aboutdlg.h>
#include "ConfigDlg.h"
#endif

#include "Common.h"
#include "Config.h"
#include "StringUtil.h"

#include "pluginspecs_wiimote.h"

#include "EmuMain.h"
#include "wiimote_real.h"

#include "Console.h" // for startConsoleWin, wprintf, GetConsoleHwnd

SWiimoteInitialize g_WiimoteInitialize;
#define VERSION_STRING "0.1"

bool g_UseRealWiiMote = false;

HINSTANCE g_hInstance;

class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp) 

WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);

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
//******************************************************************************
// Exports
//******************************************************************************
extern "C" void GetDllInfo (PLUGIN_INFO* _PluginInfo) 
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_WIIMOTE;
#ifdef DEBUGFAST 
	sprintf(_PluginInfo->Name, "Wiimote Test (DebugFast) " VERSION_STRING);
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Wiimote Test " VERSION_STRING);
#else
	sprintf(_PluginInfo->Name, "Wiimote Test (Debug) " VERSION_STRING);
#endif
#endif
}


extern "C" void DllAbout(HWND _hParent) 
{
#if !defined(OSX64)
	wxAboutDialogInfo info;
	info.SetName(_T("Wiimote test plugin"));
	info.AddDeveloper(_T("masken (masken3@gmail.com)"));
	info.SetDescription(_T("Wiimote test plugin"));
	wxAboutBox(info);
#endif
}

extern "C" void DllConfig(HWND _hParent)
{
	wxWindow win;
#ifdef _WIN32
	win.SetHWND(_hParent);
#endif
	ConfigDialog frame(&win);
	frame.ShowModal();
#ifdef _WIN32
	win.SetHWND(0);
#endif
}

extern "C" void Wiimote_Initialize(SWiimoteInitialize _WiimoteInitialize)
{
	g_WiimoteInitialize = _WiimoteInitialize;

	/* We will run WiiMoteReal::Initialize() even if we are not using a real wiimote,
	   we will initiate wiiuse.dll, but we will return before creating a new thread
	   for it in that case */
	g_UseRealWiiMote = WiiMoteReal::Initialize() > 0;

	WiiMoteEmu::Initialize();

	g_Config.Load(); // load config settings

	// Debugging window
	/*startConsoleWin(100, 750, "Wiimote"); // give room for 20 rows
	wprintf("Wiimote console opened\n");

	 // move window, TODO: make this
	//MoveWindow(GetConsoleHwnd(), 0,400, 100*8,10*14, true); // small window
	MoveWindow(GetConsoleHwnd(), 400,0, 100*8,70*14, true); // big window*/
}


extern "C" void Wiimote_DoState(void* ptr, int mode) 
{
	WiiMoteReal::DoState(ptr, mode);
	WiiMoteEmu::DoState(ptr, mode);
}

extern "C" void Wiimote_Shutdown(void) 
{
	WiiMoteReal::Shutdown();
	WiiMoteEmu::Shutdown();
}

extern "C" void Wiimote_InterruptChannel(u16 _channelID, const void* _pData, u32 _Size) 
{	
	LOGV(WII_IPC_WIIMOTE, 0, "=============================================================");
	const u8* data = (const u8*)_pData;

	// Debugging. Dump raw data.
	{
		LOG(WII_IPC_WIIMOTE, "Wiimote_Input");
		LOG(WII_IPC_WIIMOTE, "   Channel ID: %04x", _channelID);
		
		std::string Temp;
		for (u32 j=0; j<_Size; j++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", data[j]);
			Temp.append(Buffer);
		}
		LOG(WII_IPC_WIIMOTE, "   Data: %s", Temp.c_str());
	}

	if (g_UseRealWiiMote)
		WiiMoteReal::InterruptChannel(_channelID, _pData, _Size);
	else
		WiiMoteEmu::InterruptChannel(_channelID, _pData, _Size);
	LOGV(WII_IPC_WIIMOTE, 0, "=============================================================");
}

extern "C" void Wiimote_ControlChannel(u16 _channelID, const void* _pData, u32 _Size) 
{
	const u8* data = (const u8*)_pData;
	// dump raw data
	{
		LOG(WII_IPC_WIIMOTE, "Wiimote_ControlChannel");
		std::string Temp;
		for (u32 j=0; j<_Size; j++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", data[j]);
			Temp.append(Buffer);
		}
		LOG(WII_IPC_WIIMOTE, "   Data: %s", Temp.c_str());
	}

	if (g_UseRealWiiMote)
		WiiMoteReal::ControlChannel(_channelID, _pData, _Size);
	else
		WiiMoteEmu::ControlChannel(_channelID, _pData, _Size);
}

extern "C" void Wiimote_Update() 
{
	if (g_UseRealWiiMote)
		WiiMoteReal::Update();
	else
		WiiMoteEmu::Update();
}

extern "C" unsigned int Wiimote_GetAttachedControllers() 
{
	return 1;
}


// ===================================================
/* Logging functions. */
// ----------------
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
// ================
