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


#include "GCPad.h"
#include "Config.h"
#include "LogManager.h"
#if defined(HAVE_WX) && HAVE_WX
	#include "ConfigBox.h"
#endif

#ifdef _WIN32
#include "XInput.h"
#endif

// Declare config window so that we can write debugging info to it from functions in this file
#if defined(HAVE_WX) && HAVE_WX
	GCPadConfigDialog* m_ConfigFrame = NULL;
#endif


// Variables
// ---------
bool KeyStatus[LAST_CONSTANT];
bool g_SearchDeviceDone = false;
CONTROLLER_MAPPING_GC GCMapping[4];
std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
int NumPads = 0, NumGoodPads = 0, g_ID = 0;
#ifdef _WIN32
	HWND m_hWnd = NULL; // Handle to window
#endif
#if defined(HAVE_X11) && HAVE_X11
   Display* WMdisplay;
#endif
SPADInitialize *g_PADInitialize = NULL;
PLUGIN_GLOBALS* globals = NULL;


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


// Input Plugin Functions (from spec's)
// ------------------------------------

// Get properties of plugin
// ------------------------
void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

#ifdef DEBUGFAST
	sprintf(_PluginInfo->Name, "Dolphin GCPad Plugin (DebugFast)");
#else
#ifdef _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin GCPad Plugin (Debug)");
#else
	sprintf(_PluginInfo->Name, "Dolphin GCPad Plugin");
#endif
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager *)globals->logManager);
}


// Call config dialog
// ------------------
void DllConfig(HWND _hParent)
{
	if (!g_SearchDeviceDone)
	{
		g_Config.Load();	// load settings
		// Init Joystick + Haptic (force feedback) subsystem on SDL 1.3
		// Populate joyinfo for all attached devices
		Search_Devices(joyinfo, NumPads, NumGoodPads);
		g_SearchDeviceDone = true;
	}

#if defined(HAVE_WX) && HAVE_WX
	if (!m_ConfigFrame)
	{
		m_ConfigFrame = new GCPadConfigDialog(GetParentedWxWindow(_hParent));
		m_ConfigFrame->ShowModal();
		m_ConfigFrame->Destroy();
		m_ConfigFrame = NULL;
	}
#endif
}

void DllDebugger(HWND _hParent, bool Show)
{
}


// Init PAD (start emulation)
// --------------------------
void Initialize(void *init)
{
	INFO_LOG(PAD, "Initialize: %i", SDL_WasInit(0));
    g_PADInitialize = (SPADInitialize*)init;
	
#ifdef _WIN32
	m_hWnd = (HWND)g_PADInitialize->hWnd;
#endif
#if defined(HAVE_X11) && HAVE_X11
   WMdisplay = (Display*)g_PADInitialize->hWnd; 
#endif

	if (!g_SearchDeviceDone)
	{
		g_Config.Load();	// load settings
		// Populate joyinfo for all attached devices
		Search_Devices(joyinfo, NumPads, NumGoodPads);
		g_SearchDeviceDone = true;
	}
}

// Shutdown PAD (stop emulation)
// -----------------------------
void Shutdown()
{
	INFO_LOG(PAD, "Shutdown: %i", SDL_WasInit(0));

	Close_Devices();

	// Finally close SDL
	if (SDL_WasInit(0))
		SDL_Quit();

	// Remove the pointer to the initialize data
	g_PADInitialize = NULL;
	g_SearchDeviceDone = false;
}

// Save state
// --------------
void DoState(unsigned char **ptr, int mode)
{
#ifdef RERECORDING
	Recording::DoState(ptr, mode);
#endif
}

void EmuStateChange(PLUGIN_EMUSTATE newState)
{
}

// Set buttons status from keyboard input. Currently this is done from wxWidgets in the main application.
// --------------
void PAD_Input(u16 _Key, u8 _UpDown)
{
}

// Set PAD status
// --------------
// Called from: SI_DeviceGCController.cpp
// Function: Gives the current pad status to the Core
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	// Check if all is okay
	if (_pPADStatus == NULL) return;

	// Clear pad
	memset(_pPADStatus, 0, sizeof(SPADStatus));
	const int base = 0x80;
	_pPADStatus->stickX = base;
	_pPADStatus->stickY = base;
	_pPADStatus->substickX = base;
	_pPADStatus->substickY = base;
	_pPADStatus->button |= PAD_USE_ORIGIN;
	_pPADStatus->err = PAD_ERR_NONE;

	// Check that Dolphin is in focus, otherwise don't update the pad status
	if (!IsFocus()) return;

	g_ID = _numPAD;

	ReadLinuxKeyboard();
	if (NumGoodPads && NumPads > GCMapping[_numPAD].ID)
		UpdatePadState(GCMapping[_numPAD]);

	if (IsKey(EGC_A))
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = DEF_BUTTON_FULL;
	}
	if (IsKey(EGC_B))
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = DEF_BUTTON_FULL;
	}
	if (IsKey(EGC_X))	_pPADStatus->button |= PAD_BUTTON_X;
	if (IsKey(EGC_Y))	_pPADStatus->button |= PAD_BUTTON_Y;
	if (IsKey(EGC_Z))	_pPADStatus->button |= PAD_TRIGGER_Z;
	if (IsKey(EGC_START))	_pPADStatus->button |= PAD_BUTTON_START;
	if (IsKey(EGC_DPAD_UP))	_pPADStatus->button |= PAD_BUTTON_UP;
	if (IsKey(EGC_DPAD_DOWN))	_pPADStatus->button |= PAD_BUTTON_DOWN;
	if (IsKey(EGC_DPAD_LEFT))	_pPADStatus->button |= PAD_BUTTON_LEFT;
	if (IsKey(EGC_DPAD_RIGHT))	_pPADStatus->button |= PAD_BUTTON_RIGHT;

	if (GCMapping[_numPAD].Stick.Main == FROM_KEYBOARD)
	{
		bool bUp = false;
		bool bDown = false;
		bool bLeft = false;
		bool bRight = false;
		if (IsKey(EGC_STICK_UP)) bUp = true;
		else if (IsKey(EGC_STICK_DOWN)) bDown = true;
		if (IsKey(EGC_STICK_LEFT)) bLeft = true;
		else if (IsKey(EGC_STICK_RIGHT)) bRight = true;
		EmulateAnalogStick(_pPADStatus->stickX, _pPADStatus->stickY, bUp, bDown, bLeft, bRight, DEF_STICK_FULL);
	}
	else if (GCMapping[_numPAD].Stick.Main == FROM_ANALOG1)
	{
		_pPADStatus->stickX = GCMapping[_numPAD].AxisState.Lx;
		// Y-axis is inverted
		_pPADStatus->stickY = 0xFF - (u8)GCMapping[_numPAD].AxisState.Ly;
	}
	else if (GCMapping[_numPAD].Stick.Main == FROM_ANALOG2)
	{
		_pPADStatus->stickX = GCMapping[_numPAD].AxisState.Rx;
		// Y-axis is inverted
		_pPADStatus->stickY = 0xFF - (u8)GCMapping[_numPAD].AxisState.Ry;
	}
	else
	{
		_pPADStatus->stickX = GCMapping[_numPAD].AxisState.Tl;
		_pPADStatus->stickY = GCMapping[_numPAD].AxisState.Tr;
	}

	if (GCMapping[_numPAD].Stick.Sub == FROM_KEYBOARD)
	{
		bool bUp = false;
		bool bDown = false;
		bool bLeft = false;
		bool bRight = false;
		if (IsKey(EGC_CSTICK_UP)) bUp = true;
		else if (IsKey(EGC_CSTICK_DOWN)) bDown = true;
		if (IsKey(EGC_CSTICK_LEFT)) bLeft = true;
		else if (IsKey(EGC_CSTICK_RIGHT)) bRight = true;
		EmulateAnalogStick(_pPADStatus->substickX, _pPADStatus->substickY, bUp, bDown, bLeft, bRight, DEF_STICK_FULL);
	}
	else if (GCMapping[_numPAD].Stick.Sub == FROM_ANALOG1)
	{
		_pPADStatus->substickX = GCMapping[_numPAD].AxisState.Lx;
		// Y-axis is inverted
		_pPADStatus->substickY = 0xFF - (u8)GCMapping[_numPAD].AxisState.Ly;
	}
	else if (GCMapping[_numPAD].Stick.Sub == FROM_ANALOG2)
	{
		_pPADStatus->substickX = GCMapping[_numPAD].AxisState.Rx;
		// Y-axis is inverted
		_pPADStatus->substickY = 0xFF - (u8)GCMapping[_numPAD].AxisState.Ry;
	}
	else
	{
		_pPADStatus->substickX = GCMapping[_numPAD].AxisState.Tl;
		_pPADStatus->substickY = GCMapping[_numPAD].AxisState.Tr;
	}

	if (GCMapping[_numPAD].Stick.Shoulder == FROM_KEYBOARD)
	{
		if (IsKey(EGC_TGR_L))
		{
			_pPADStatus->button |= PAD_TRIGGER_L;
			_pPADStatus->triggerLeft = DEF_TRIGGER_FULL;
		}
		else if (IsKey(EGC_TGR_SEMI_L))
		{
			_pPADStatus->triggerLeft = DEF_TRIGGER_FULL / 2;
		}

		if (IsKey(EGC_TGR_R))
		{
			_pPADStatus->button |= PAD_TRIGGER_R;
			_pPADStatus->triggerRight = DEF_TRIGGER_FULL;
		}
		else if (IsKey(EGC_TGR_SEMI_R))
		{
			_pPADStatus->triggerRight = DEF_TRIGGER_FULL / 2;
		}
	}
	else if (GCMapping[_numPAD].Stick.Shoulder == FROM_ANALOG1)
	{
		_pPADStatus->triggerLeft = GCMapping[_numPAD].AxisState.Lx;
		_pPADStatus->triggerRight = GCMapping[_numPAD].AxisState.Ly;
		if (_pPADStatus->triggerLeft > DEF_TRIGGER_THRESHOLD)
			_pPADStatus->button |= PAD_TRIGGER_L;
		if (_pPADStatus->triggerRight > DEF_TRIGGER_THRESHOLD)
			_pPADStatus->button |= PAD_TRIGGER_R;
	}
	else if (GCMapping[_numPAD].Stick.Shoulder == FROM_ANALOG2)
	{
		_pPADStatus->triggerLeft = GCMapping[_numPAD].AxisState.Rx;
		_pPADStatus->triggerRight = GCMapping[_numPAD].AxisState.Ry;
		if (_pPADStatus->triggerLeft > DEF_TRIGGER_THRESHOLD)
			_pPADStatus->button |= PAD_TRIGGER_L;
		if (_pPADStatus->triggerRight > DEF_TRIGGER_THRESHOLD)
			_pPADStatus->button |= PAD_TRIGGER_R;
	}
	else
	{
		_pPADStatus->triggerLeft = GCMapping[_numPAD].AxisState.Tl;
		_pPADStatus->triggerRight = GCMapping[_numPAD].AxisState.Tr;
		if (_pPADStatus->triggerLeft > DEF_TRIGGER_THRESHOLD)
			_pPADStatus->button |= PAD_TRIGGER_L;
		if (_pPADStatus->triggerRight > DEF_TRIGGER_THRESHOLD)
			_pPADStatus->button |= PAD_TRIGGER_R;
	}

}


//******************************************************************************
// Supporting functions
//******************************************************************************

// for same displacement should be sqrt(2)/2 (in theory)
// 3/4 = 0.75 is a little faster than sqrt(2)/2 = 0.7071...
// In SMS, 17/20 = 0.85 is perfect; in WW, 7/10 = 0.70 is closer.
#define DIAGONAL_SCALE 0.70710678
void EmulateAnalogStick(unsigned char &stickX, unsigned char &stickY, bool buttonUp, bool buttonDown, bool buttonLeft, bool buttonRight, int magnitude)
{
	int mainX = 0;
	int mainY = 0;
	if (buttonUp)
		mainY = magnitude;
	else if (buttonDown)
		mainY = -magnitude;
	if (buttonLeft)
		mainX = -magnitude;
	else if (buttonRight)
		mainX = magnitude;

	if ((mainX == 0) || (mainY == 0))
	{
		stickX += mainX;
		stickY += mainY;
	}
	else
	{
		stickX += mainX * DIAGONAL_SCALE;
		stickY += mainY * DIAGONAL_SCALE;
	}
}

void Close_Devices()
{
	PAD_RumbleClose();

	if (SDL_WasInit(0))
	{
		for (int i = 0; i < NumPads; i++)
		{
			if (joyinfo.at(i).joy)
			{
				if(SDL_JoystickOpened(i))
				{
					INFO_LOG(WIIMOTE, "Shut down Joypad: %i", i);
					SDL_JoystickClose(joyinfo.at(i).joy);
				}
			}
		}
	}

	for (int i = 0; i < 4; i++)
		GCMapping[i].joy = NULL;

	// Clear the physical device info
	joyinfo.clear();
	NumPads = 0;
	NumGoodPads = 0;
}

// Search for SDL devices
// ----------------
bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads)
{
	// Close opened devices first
	Close_Devices();

	bool success = InputCommon::SearchDevices(_joyinfo, _NumPads, _NumGoodPads);

	if (_NumGoodPads == 0)
		return false;

	for (int i = 0; i < 4; i++)
	{
		if (_NumPads > GCMapping[i].ID)
			if(joyinfo.at(GCMapping[i].ID).Good)
			{
				GCMapping[i].joy = joyinfo.at(GCMapping[i].ID).joy;
#ifdef _WIN32
				XINPUT_STATE xstate;
				DWORD xresult = XInputGetState(GCMapping[i].ID, &xstate);
				if (xresult == ERROR_SUCCESS)
					GCMapping[i].TriggerType = InputCommon::CTL_TRIGGER_XINPUT;
#endif
			}
	}

	return success;
}

void GetAxisState(CONTROLLER_MAPPING_GC &_GCMapping)
{
	// Update the gamepad status
	SDL_JoystickUpdate();

	// Update axis states. It doesn't hurt much if we happen to ask for nonexisting axises here.
	_GCMapping.AxisState.Lx = SDL_JoystickGetAxis(_GCMapping.joy, _GCMapping.AxisMapping.Lx);
	_GCMapping.AxisState.Ly = SDL_JoystickGetAxis(_GCMapping.joy, _GCMapping.AxisMapping.Ly);
	_GCMapping.AxisState.Rx = SDL_JoystickGetAxis(_GCMapping.joy, _GCMapping.AxisMapping.Rx);
	_GCMapping.AxisState.Ry = SDL_JoystickGetAxis(_GCMapping.joy, _GCMapping.AxisMapping.Ry);

	// Update the analog trigger axis values
#ifdef _WIN32
	if (_GCMapping.TriggerType == InputCommon::CTL_TRIGGER_SDL)
	{
#endif
		// If we are using SDL analog triggers the buttons have to be mapped as 1000 or up, otherwise they are not used
		// We must also check that we are not asking for a negative axis number because SDL_JoystickGetAxis() has
		// no good way of handling that
		if ((_GCMapping.AxisMapping.Tl - 1000) >= 0)
			_GCMapping.AxisState.Tl = SDL_JoystickGetAxis(_GCMapping.joy, _GCMapping.AxisMapping.Tl - 1000);
		if ((_GCMapping.AxisMapping.Tr - 1000) >= 0)
			_GCMapping.AxisState.Tr = SDL_JoystickGetAxis(_GCMapping.joy, _GCMapping.AxisMapping.Tr - 1000);
#ifdef _WIN32
	}
	else
	{
		_GCMapping.AxisState.Tl = XInput::GetXI(_GCMapping.ID, _GCMapping.AxisMapping.Tl - 1000);
		_GCMapping.AxisState.Tr = XInput::GetXI(_GCMapping.ID, _GCMapping.AxisMapping.Tr - 1000);
	}
#endif
}

void UpdatePadState(CONTROLLER_MAPPING_GC &_GCiMapping)
{
	// Return if we have no pads
	if (NumGoodPads == 0) return;

	GetAxisState(_GCiMapping);

	int &Lx = _GCiMapping.AxisState.Lx;
	int &Ly = _GCiMapping.AxisState.Ly;
	int &Rx = _GCiMapping.AxisState.Rx;
	int &Ry = _GCiMapping.AxisState.Ry;
	int &Tl = _GCiMapping.AxisState.Tl;
	int &Tr = _GCiMapping.AxisState.Tr;

	// Check the circle to square option
	if(_GCiMapping.bSquare2Circle)
	{
		InputCommon::Square2Circle(Lx, Ly, _GCiMapping.Diagonal, false);
		InputCommon::Square2Circle(Rx, Ry, _GCiMapping.Diagonal, false);
	}

	// Dead zone adjustment
	float DeadZoneLeft = (float)_GCiMapping.DeadZoneL / 100.0f;
	float DeadZoneRight = (float)_GCiMapping.DeadZoneR / 100.0f;
	if (InputCommon::IsDeadZone(DeadZoneLeft, Lx, Ly))
	{
		Lx = 0;
		Ly = 0;
	}
	if (InputCommon::IsDeadZone(DeadZoneRight, Rx, Ry))
	{
		Rx = 0;
		Ry = 0;
	}

	// Downsize the values from 0x8000 to 0x80
	Lx = InputCommon::Pad_Convert(Lx);
	Ly = InputCommon::Pad_Convert(Ly);
	Rx = InputCommon::Pad_Convert(Rx);
	Ry = InputCommon::Pad_Convert(Ry);
	// The XInput range is already 0 to 0x80
	if (_GCiMapping.TriggerType == InputCommon::CTL_TRIGGER_SDL)
	{
		Tl = InputCommon::Pad_Convert(Tl);
		Tr = InputCommon::Pad_Convert(Tr);
	}
}

// Multi System Input Status Check
bool IsKey(int Key)
{
	int Ret = NULL;
	int MapKey = GCMapping[g_ID].Button[Key];

	if (MapKey < 256)
	{
#ifdef _WIN32
		Ret = GetAsyncKeyState(MapKey);		// Keyboard (Windows)
#else
		Ret = KeyStatus[Key];			// Keyboard (Linux)
#endif
	}
	else if (MapKey < 0x1100)
	{
		Ret = SDL_JoystickGetButton(GCMapping[g_ID].joy, MapKey - 0x1000);	// Pad button
	}
	else	// Pad hat
	{
		u8 HatCode, HatKey;
		HatCode = SDL_JoystickGetHat(GCMapping[g_ID].joy, (MapKey - 0x1100) / 0x0010);
		HatKey = (MapKey - 0x1100) % 0x0010;

		if (HatCode & HatKey)
			Ret = HatKey;
	}

	return (Ret) ? true : false;
}

void ReadLinuxKeyboard()
{
#if defined(HAVE_X11) && HAVE_X11
	XEvent E;
	KeySym key;

	// keyboard input
	int num_events;
	for (num_events = XPending(WMdisplay); num_events > 0; num_events--)
	{
		XNextEvent(WMdisplay, &E);
		switch (E.type)
		{
		case KeyPress:
		{
			key = XLookupKeysym((XKeyEvent*)&E, 0);
			
			if ((key >= XK_F1 && key <= XK_F9) ||
			   key == XK_Shift_L || key == XK_Shift_R ||
			   key == XK_Control_L || key == XK_Control_R)
			{
				XPutBackEvent(WMdisplay, &E);
				break;
			}

			for (int i = 0; i < LAST_CONSTANT; i++)
			{
				if (((int) key) == GCMapping[g_ID].Button[i])
					KeyStatus[i] = true;
			}
			break;
		}
		case KeyRelease:
		{
			key = XLookupKeysym((XKeyEvent*)&E, 0);
			
			if ((key >= XK_F1 && key <= XK_F9) ||
			   key == XK_Shift_L || key == XK_Shift_R ||
			   key == XK_Control_L || key == XK_Control_R) {
				XPutBackEvent(WMdisplay, &E);
				break;
			}

			for (int i = 0; i < LAST_CONSTANT; i++)
			{
				if (((int) key) == GCMapping[g_ID].Button[i])
					KeyStatus[i] = false;
			}
			break;
		}
		default:
			break;
		}
	}
#endif
}

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
#else
	return true;
#endif
}
