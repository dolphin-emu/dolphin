
#include "Common.h"
#include "pluginspecs_wiimote.h"

#include "WiimoteReal/WiimoteReal.h"

#include "ControllerInterface/ControllerInterface.h"
#include "WiimoteEmu/WiimoteEmu.h"

#if defined(HAVE_WX) && HAVE_WX
#include "WiimoteConfigDiag.h"
#include "../../InputUICommon/Src/ConfigDiag.h"
#endif
#include "../../InputCommon/Src/InputConfig.h"

#if defined(HAVE_X11) && HAVE_X11
#include <X11/Xlib.h>
#if defined(HAVE_WX) && HAVE_WX
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#endif
#endif

#define PLUGIN_VERSION		0x0100

#define PLUGIN_NAME		"Dolphin Wiimote New Incomplete"
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
InputPlugin g_plugin( "WiimoteNew", "Wiimote", "Wiimote" );
SWiimoteInitialize g_WiimoteInitialize;

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
	if (g_plugin.controller_interface.IsInit())
	{
		std::vector<ControllerEmu*>::const_iterator
			i = g_plugin.controllers.begin(),
			e = g_plugin.controllers.end();
		for ( ; i!=e; ++i )
			delete *i;
		g_plugin.controllers.clear();

		// true parameter to make SDL not quit in the wiimote plugin,
		// the old wiimote plugin uses this hack as well, to prevent crash on stop
		g_plugin.controller_interface.DeInit(true);

		// real wiimotes
		WiimoteReal::Shutdown();
	}
}

// if plugin isn't initialized, init and load config
void InitPlugin( void* const hwnd )
{
	if (false == g_plugin.controller_interface.IsInit())
	{
		// add 4 wiimotes
		for ( unsigned int i = 0; i<4; ++i )
			g_plugin.controllers.push_back( new WiimoteEmu::Wiimote(i) );

		// needed for Xlib and exclusive dinput
		g_plugin.controller_interface.SetHwnd(hwnd);
		g_plugin.controller_interface.Init();

		// load the saved controller config
		g_plugin.LoadConfig();

		// real wiimotes
		WiimoteReal::LoadSettings();
		WiimoteReal::Initialize();
	}
}

// I N T E R F A C E 

// __________________________________________________________________________________________________
// Function: Wiimote_Output
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID CONTROL channel.
// input:    Da pakket.
// output:   none
//
void Wiimote_ControlChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	switch (g_wiimote_sources[_number])
	{
	case WIIMOTE_SRC_REAL :
		WiimoteReal::ControlChannel(_number, _channelID, _pData, _Size);
		break;
	case WIIMOTE_SRC_EMU :
		((WiimoteEmu::Wiimote*)g_plugin.controllers[ _number ])->ControlChannel( _channelID, _pData, _Size );
		break;
	}
}

// __________________________________________________________________________________________________
// Function: Send keyboard input to the plugin
// Purpose:
// input:   The key and if it's pressed or released
// output:  None
//
void Wiimote_Input(u16 _Key, u8 _UpDown)
{
	return;
}

// __________________________________________________________________________________________________
// Function: Unpair real Wiimotes (just a dummy to be compliant)
// Purpose: 
// input:   None
// output:  0 Wiimotes disconnected
//
unsigned int Wiimote_UnPairWiimotes(void)
{
	return 0;
}

// __________________________________________________________________________________________________
// Function: Wiimote_InterruptChannel
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID INTERRUPT channel.
// input:    Da pakket.
// output:   none
//
void Wiimote_InterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	switch (g_wiimote_sources[_number])
	{
	case WIIMOTE_SRC_REAL :
		WiimoteReal::InterruptChannel(_number, _channelID, _pData, _Size);
		break;
	case WIIMOTE_SRC_EMU :
		((WiimoteEmu::Wiimote*)g_plugin.controllers[ _number ])->InterruptChannel( _channelID, _pData, _Size );
		break;
	}
}

// __________________________________________________________________________________________________
// Function: Wiimote_Update
// Purpose:  This function is called periodically by the Core.
// input:    none
// output:   none
//
void Wiimote_Update(int _number)
{
	//PanicAlert( "Wiimote_Update" );

	// TODO: change this to a TryEnter, and make it give empty input on failure
	g_plugin.controls_crit.Enter();

	static int _last_number = 4;
	if ( _number <= _last_number && g_plugin.interface_crit.TryEnter() )
	{
		g_plugin.controller_interface.UpdateOutput();
		g_plugin.controller_interface.UpdateInput();
		g_plugin.interface_crit.Leave();
	}
	_last_number = _number;

	switch (g_wiimote_sources[_number])
	{
	case WIIMOTE_SRC_REAL :
		WiimoteReal::Update(_number);
		break;
	case WIIMOTE_SRC_EMU :
		((WiimoteEmu::Wiimote*)g_plugin.controllers[ _number ])->Update();
		break;
	}

	g_plugin.controls_crit.Leave();
}

// __________________________________________________________________________________________________
// Function: PAD_GetAttachedPads
// Purpose:  Get mask of attached pads (eg: controller 1 & 4 -> 0x9)
// input:	 none
// output:   number of pads
//
unsigned int Wiimote_GetAttachedControllers()
{
	unsigned int attached = 0;
	for (unsigned int i=0; i<4; ++i)
		if (g_wiimote_sources[i])
			attached |= (1 << i);
	return attached;
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
	_pPluginInfo->Type = PLUGIN_TYPE_WIIMOTE;
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
#if defined(HAVE_WX) && HAVE_WX
	const bool was_init = g_plugin.controller_interface.IsInit();

	if (false == was_init)
	{
#if defined(HAVE_X11) && HAVE_X11
		Window win = GDK_WINDOW_XID(GTK_WIDGET(_hParent)->window);
		g_WiimoteInitialize.hWnd = GDK_WINDOW_XDISPLAY(GTK_WIDGET(_hParent)->window);
		g_WiimoteInitialize.pXWindow = &win;
		InitPlugin(g_WiimoteInitialize.hWnd);
#else
		InitPlugin(_hParent);
#endif
	}

	wxWindow *frame = GetParentedWxWindow(_hParent);
	WiimoteConfigDiag* const m_ConfigFrame = new WiimoteConfigDiag(frame, g_plugin);
	//InputConfigDialog* const m_ConfigFrame = new InputConfigDialog(frame, g_plugin, PLUGIN_FULL_NAME);

#ifdef _WIN32
	frame->Disable();
	m_ConfigFrame->ShowModal();
	frame->Enable();
#else
	m_ConfigFrame->ShowModal();
#endif

#ifdef _WIN32
	wxMilliSleep(50);	// hooray for hacks
	frame->SetFocus();
	frame->SetHWND(NULL);
#endif

	m_ConfigFrame->Destroy();
	frame->Destroy();
	// /

	if (false == was_init)
		DeInitPlugin();
#endif
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
	g_WiimoteInitialize = *(SWiimoteInitialize*)init;
	if ( false == g_plugin.controller_interface.IsInit() )
		InitPlugin( g_WiimoteInitialize.hWnd );
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
	// TODO: this

	//PointerWrap p(ptr, mode);
	//for (unsigned int i=0; i<4; ++i)
	//	((WiimoteEmu::Wiimote*)g_plugin.controllers[i])->DoState(p);
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
	WiimoteReal::StateChange(newState);
}
