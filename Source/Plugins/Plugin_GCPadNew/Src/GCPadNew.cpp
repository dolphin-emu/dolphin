// Copyright (C) 2010 Dolphin Project.

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
#include "pluginspecs_pad.h"

#include "ControllerInterface/ControllerInterface.h"
#include "GCPadEmu.h"

#if defined(HAVE_WX) && HAVE_WX
#include "../../InputUICommon/Src/ConfigDiag.h"
#endif
#include "../../InputCommon/Src/InputConfig.h"

#if defined(HAVE_X11) && HAVE_X11
#include <X11/Xlib.h>
#endif

#define PLUGIN_VERSION		0x0100

#define PLUGIN_NAME		"Dolphin GCPad New"
#ifdef DEBUGFAST
#define PLUGIN_FULL_NAME	PLUGIN_NAME" (DebugFast)"
#else
#ifdef _DEBUG
#define PLUGIN_FULL_NAME	PLUGIN_NAME" (Debug)"
#else
#define PLUGIN_FULL_NAME	PLUGIN_NAME
#endif
#endif

// plugin globals
static InputPlugin g_plugin( "GCPadNew", "Pad", "GCPad" );
SPADInitialize *g_PADInitialize = NULL;

#ifdef _WIN32
class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	};
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp)
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

// copied from GCPad
HINSTANCE g_hInstance;

// copied from GCPad
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
// /

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		wxSetInstance(hinstDLL);
		wxInitialize();
		break; 
	case DLL_PROCESS_DETACH:
		wxUninitialize();
		break;
	default:
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}
#endif

void DeInitPlugin()
{
	// i realize i am checking IsInit() twice, just too lazy to change it
	if ( g_plugin.controller_interface.IsInit() )
	{
		std::vector<ControllerEmu*>::const_iterator
			i = g_plugin.controllers.begin(),
			e = g_plugin.controllers.end();
		for ( ; i!=e; ++i )
			delete *i;
		g_plugin.controllers.clear();

		g_plugin.controller_interface.DeInit();
	}
}

// if plugin isn't initialized, init and load config
void InitPlugin( void* const hwnd )
{
	// i realize i am checking IsInit() twice, just too lazy to change it
	if ( false == g_plugin.controller_interface.IsInit() )
	{
		// add 4 gcpads
		for ( unsigned int i = 0; i<4; ++i )
			g_plugin.controllers.push_back( new GCPad( i ) );
		
		// needed for Xlib
		g_plugin.controller_interface.SetHwnd(hwnd);
		g_plugin.controller_interface.Init();

		// load the saved controller config
		if (false == g_plugin.LoadConfig())
		{
			// load default config for pad 1
			g_plugin.controllers[0]->LoadDefaults();

			// kinda silly, set default device(all controls) to first one found in ControllerInterface
			// should be the keyboard device
			if (g_plugin.controller_interface.Devices().size())
			{
				g_plugin.controllers[0]->default_device.FromDevice(g_plugin.controller_interface.Devices()[0]);
				g_plugin.controllers[0]->UpdateDefaultDevice();
			}
		}

		// update control refs
		std::vector<ControllerEmu*>::const_iterator
			i = g_plugin.controllers.begin(),
			e = g_plugin.controllers.end();
		for ( ; i!=e; ++i )
			(*i)->UpdateReferences( g_plugin.controller_interface );

	}
}

// I N T E R F A C E 

// __________________________________________________________________________________________________
// Function:
// Purpose:  
// input:   
// output:   
//
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	memset( _pPADStatus, 0, sizeof(*_pPADStatus) );
	_pPADStatus->err = PAD_ERR_NONE;
	// wtf is this?	
	_pPADStatus->button |= PAD_USE_ORIGIN;

	// try lock
	if ( false == g_plugin.controls_crit.TryEnter() )
	{
		// if gui has lock (messing with controls), skip this input cycle
		// center axes and return
		memset( &_pPADStatus->stickX, 0x80, 4 );
		return;
	}

	// if we are on the next input cycle, update output and input
	// if we can get a lock
	static int _last_numPAD = 4;
	if ( _numPAD <= _last_numPAD && g_plugin.interface_crit.TryEnter() )
	{
		g_plugin.controller_interface.UpdateOutput();
		g_plugin.controller_interface.UpdateInput();
		g_plugin.interface_crit.Leave();
	}
	_last_numPAD = _numPAD;
	
	// get input
	((GCPad*)g_plugin.controllers[ _numPAD ])->GetInput( _pPADStatus );

	// leave
	g_plugin.controls_crit.Leave();

}

// __________________________________________________________________________________________________
// Function: Send keyboard input to the plugin
// Purpose:  
// input:   The key and if it's pressed or released
// output:  None
//
void PAD_Input(u16 _Key, u8 _UpDown)
{
	// nofin
}

// __________________________________________________________________________________________________
// Function: PAD_Rumble
// Purpose:  Pad rumble!
// input:	 PAD number, Command type (Stop=0, Rumble=1, Stop Hard=2) and strength of Rumble
// output:   none
//
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	// enter
	if ( g_plugin.controls_crit.TryEnter() )
	{
		// TODO: this has potential to not stop rumble if user is messing with GUI at the perfect time
		// set rumble
		((GCPad*)g_plugin.controllers[ _numPAD ])->SetOutput( 1 == _uType && _uStrength > 2 );

		// leave
		g_plugin.controls_crit.Leave();
	}
}


// GLOBAL I N T E R F A C E 
// Function: GetDllInfo
// Purpose:  This function allows the emulator to gather information
//           about the DLL by filling in the PluginInfo structure.
// input:    A pointer to a PLUGIN_INFO structure that needs to be
//           filled by the function. (see def above)
// output:   none
//
void GetDllInfo(PLUGIN_INFO* _pPluginInfo)
{
	// don't feel like messing around with all those strcpy functions and warnings
	//char *s1 = CIFACE_PLUGIN_FULL_NAME, *s2 = _pPluginInfo->Name;
	//while ( *s2++ = *s1++ );
	memcpy( _pPluginInfo->Name, PLUGIN_FULL_NAME, sizeof(PLUGIN_FULL_NAME) );
	_pPluginInfo->Type = PLUGIN_TYPE_PAD;
	_pPluginInfo->Version = PLUGIN_VERSION;
}

// ___________________________________________________________________________
// Function: DllConfig
// Purpose:  This function is optional function that is provided
//           to allow the user to configure the DLL
// input:    A handle to the window that calls this function
// output:   none
//
void DllConfig(HWND _hParent)
{
	bool was_init = false;
#if defined(HAVE_X11) && HAVE_X11
	Display *dpy = NULL;
#endif
	if ( g_plugin.controller_interface.IsInit() )	// check if game is running
		was_init = true;
	else
	{
#if defined(HAVE_X11) && HAVE_X11
		dpy = XOpenDisplay(0);
		InitPlugin(dpy);
#else
		InitPlugin(_hParent);
#endif
	}

	// copied from GCPad
#if defined(HAVE_WX) && HAVE_WX
	wxWindow *frame = GetParentedWxWindow(_hParent);
	InputConfigDialog* m_ConfigFrame = new InputConfigDialog( frame, g_plugin, PLUGIN_FULL_NAME, was_init );

#ifdef _WIN32
	frame->Disable();
	m_ConfigFrame->ShowModal();
	frame->Enable();
#else
	m_ConfigFrame->ShowModal();
#endif

#ifdef _WIN32
	wxMilliSleep( 50 );	// hooray for hacks
	frame->SetFocus();
	frame->SetHWND(NULL);
#endif

	m_ConfigFrame->Destroy();
	m_ConfigFrame = NULL;
	frame->Destroy();
#endif
	// /

	if ( !was_init )				// if game is running
	{
#if defined(HAVE_X11) && HAVE_X11
		XCloseDisplay(dpy);
#endif
		DeInitPlugin();
	}
}

// ___________________________________________________________________________
// Function: DllDebugger
// Purpose:  Open the debugger
// input:    a handle to the window that calls this function
// output:   none
//
void DllDebugger(HWND _hParent, bool Show)
{
	// wut?
}

// ___________________________________________________________________________
// Function: DllSetGlobals
// Purpose:  Set the pointer for globals variables
// input:    a pointer to the global struct
// output:   none
//
void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	// wut?
}

// ___________________________________________________________________________
// Function: Initialize
// Purpose: Initialize the plugin
// input:    Init
// output:   none
//
void Initialize(void *init)
{
	g_PADInitialize = (SPADInitialize*)init;
	if ( false == g_plugin.controller_interface.IsInit() )
		InitPlugin( g_PADInitialize->hWnd );
}

// ___________________________________________________________________________
// Function: Shutdown
// Purpose:  This function is called when the emulator is shutting down
//           a game allowing the dll to de-initialise.
// input:    none
// output:   none
//
void Shutdown(void)
{
	if ( g_plugin.controller_interface.IsInit() )
		DeInitPlugin();
}

// ___________________________________________________________________________
// Function: DoState
// Purpose:  Saves/load state
// input/output: ptr
// input: mode
//
void DoState(unsigned char **ptr, int mode)
{
	// prolly won't need this
}

// ___________________________________________________________________________
// Function: EmuStateChange
// Purpose: Notifies the plugin of a change in emulation state
// input:    newState
// output:   none
//
void EmuStateChange(PLUGIN_EMUSTATE newState)
{
	// maybe use this later
}
