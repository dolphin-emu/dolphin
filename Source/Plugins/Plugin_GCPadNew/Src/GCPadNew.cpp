
#include <math.h>
#include "Common.h"
#include "pluginspecs_pad.h"
#include "pluginspecs_wiimote.h"

#include "ControllerInterface/ControllerInterface.h"
#if defined(HAVE_WX) && HAVE_WX
#include "ConfigDiag.h"
#endif
#include "Config.h"

#if defined(HAVE_X11) && HAVE_X11
#include <X11/Xlib.h>
#endif

#define PLUGIN_VERSION		0x0100

#ifdef DEBUGFAST
#define PLUGIN_FULL_NAME	"Dolphin GCPad New (DebugFast)"
#else
#ifdef _DEBUG
#define PLUGIN_FULL_NAME	"Dolphin GCPad New (Debug)"
#else
#define PLUGIN_FULL_NAME	"Dolphin GCPad New"
#endif
#endif


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
SPADInitialize *g_PADInitialize = NULL;
// Check if Dolphin is in focus
// ----------------
bool IsFocus()
{
#ifdef _WIN32
	HWND RenderingWindow = (g_PADInitialize) ? g_PADInitialize->hWnd : NULL;
	HWND Parent = GetParent(RenderingWindow);
	HWND TopLevel = GetParent(Parent);

	if (GetForegroundWindow() == TopLevel || GetForegroundWindow() == RenderingWindow)
		return true;
	else
		return false;
#elif defined HAVE_X11 && HAVE_X11
	Display* GCdisplay = (Display*)g_PADInitialize->hWnd;
	Window GLWin = *(Window *)g_PADInitialize->pXWindow;
	Window FocusWin;
	int Revert;
	XGetInputFocus(GCdisplay, &FocusWin, &Revert);
	XWindowAttributes WinAttribs;
	XGetWindowAttributes (GCdisplay, GLWin, &WinAttribs);
	return (GLWin != 0 && (GLWin == FocusWin || WinAttribs.override_redirect));
#else
	return true;
#endif
}

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

// the plugin
Plugin g_plugin;
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


int _last_numPAD = 4;


// if plugin isn't initialized, init and load config
void InitPlugin( void* const hwnd )
{
	//g_plugin.controls_crit.Enter();		// enter
	//g_plugin.interface_crit.Enter();

	if ( false == g_plugin.controller_interface.IsInit() )
	{
		// load the saved controller config
		g_plugin.LoadConfig();
		
		// needed for Xlib and exclusive dinput
		g_plugin.controller_interface.SetHwnd( hwnd );
		g_plugin.controller_interface.Init();

		// update control refs
		std::vector<ControllerEmu*>::const_iterator i = g_plugin.controllers.begin(),
			e = g_plugin.controllers.end();
		for ( ; i!=e; ++i )
			(*i)->UpdateReferences( g_plugin.controller_interface );

	}

	//g_plugin.interface_crit.Leave();
	//g_plugin.controls_crit.Leave();		// leave
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
	// why not, i guess
	if ( NULL == _pPADStatus )
		return;

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
	if ( _numPAD <= _last_numPAD && g_plugin.interface_crit.TryEnter() )
	{
		g_plugin.controller_interface.UpdateOutput();
		g_plugin.controller_interface.UpdateInput();
		g_plugin.interface_crit.Leave();
	}
	_last_numPAD = _numPAD;
	
	// if we want background input or have focus
	if ( g_plugin.controllers[_numPAD]->options[0].settings[0]->value || IsFocus() )
		// get input
		((GCPad*)g_plugin.controllers[ _numPAD ])->GetInput( _pPADStatus );
	else
	{
		// center sticks
		memset( &_pPADStatus->stickX, 0x80, 4 );
		// stop rumble
		((GCPad*)g_plugin.controllers[ _numPAD ])->SetOutput( false );
	}

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
		// only on/off rumble, if we have focus or background input on
		if ( g_plugin.controllers[_numPAD]->options[0].settings[0]->value || IsFocus() )
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
	if ( g_plugin.controller_interface.IsInit() )	// hack for showing dialog when game isnt running
		was_init = true;
	else
		InitPlugin( _hParent );

	// copied from GCPad
#if defined(HAVE_WX) && HAVE_WX
	wxWindow *frame = GetParentedWxWindow(_hParent);
	ConfigDialog* m_ConfigFrame = new ConfigDialog( frame, g_plugin, PLUGIN_FULL_NAME, was_init );

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

	if ( false == was_init )				// hack for showing dialog when game isnt running
		g_plugin.controller_interface.DeInit();
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
		InitPlugin( ((SPADInitialize*)init)->hWnd );
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
	//plugin.controls_crit.Enter();	// enter
	if ( g_plugin.controller_interface.IsInit() )
		g_plugin.controller_interface.DeInit();
	//plugin.controls_crit.Leave();	// leave
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
