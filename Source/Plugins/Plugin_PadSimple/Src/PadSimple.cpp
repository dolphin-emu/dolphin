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



// Include
#include <stdio.h>
#include <math.h>

#include "Common.h"
#include "LogManager.h"
#include "pluginspecs_pad.h"
#include "PadSimple.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "FileUtil.h"
#include "ChunkFile.h"

#if defined(HAVE_WX) && HAVE_WX
	#include "GUI/ConfigDlg.h"
	PADConfigDialogSimple* m_ConfigFrame = NULL;
#endif

#ifdef _WIN32
	#include "XInput.h"
	#include "../../../Core/InputCommon/Src/DirectInputBase.h" // Core

	DInput dinput;
	//#elif defined(USE_SDL) && USE_SDL
	//#include <SDL.h>

#elif defined(HAVE_X11) && HAVE_X11

	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
	#include <X11/keysym.h>
	#include <X11/XKBlib.h>

	Display* GXdsp;
	bool KeyStatus[NUMCONTROLS];
#elif defined(HAVE_COCOA) && HAVE_COCOA
	#include <Cocoa/Cocoa.h>
	bool KeyStatus[NUMCONTROLS];
#endif



// Declarations
SPads pad[4];
SPADInitialize g_PADInitialize;


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



// Input Recording

// Enable these to record or play back
//#define RECORD_REPLAY
//#define RECORD_STORE

// Pre defined maxium storage limit
#define RECORD_SIZE (1024 * 128)
PLUGIN_GLOBALS* globals = NULL;
SPADStatus recordBuffer[RECORD_SIZE];
int count = 0;
bool g_EmulatorRunning = false;

//******************************************************************************
// Supporting functions
//******************************************************************************

void RecordInput(const SPADStatus& _rPADStatus)
{
	if (count >= RECORD_SIZE) return;
	recordBuffer[count++] = _rPADStatus;

	// Logging
	//u8 TmpData[sizeof(SPADStatus)];
	//memcpy(TmpData, &recordBuffer[count - 1], sizeof(SPADStatus));
	//Console::Print("RecordInput(%i): %s\n", count, ArrayToString(TmpData, sizeof(SPADStatus), 0, 30).c_str());

	// Auto save every ten seconds
	if (count % (60 * 10) == 0) SaveRecord();
}

const SPADStatus& PlayRecord()
{
	// Logging
	//Console::Print("PlayRecord(%i)\n", count);

	if (count >= RECORD_SIZE)
	{
		// Todo: Make the recording size unlimited?
		//PanicAlert("The recording reached its end");
		return(recordBuffer[0]);
	}
	return(recordBuffer[count++]);
}

void LoadRecord()
{
	FILE* pStream = fopen("pad-record.bin", "rb");

	if (pStream != NULL)
	{
		fread(recordBuffer, 1, RECORD_SIZE * sizeof(SPADStatus), pStream);
		fclose(pStream);
	}
	else
	{
		PanicAlert("SimplePad: Could not open pad-record.bin");
	}

	//Console::Print("LoadRecord()");
}

void SaveRecord()
{
	// Open the file in a way that clears all old data
	FILE* pStream = fopen("pad-record.bin", "wb");

	if (pStream != NULL)
	{
		fwrite(recordBuffer, 1, RECORD_SIZE * sizeof(SPADStatus), pStream);
		fclose(pStream);
	}
	else
	{
		PanicAlert("SimplePad: Could not save pad-record.bin");
	}
	//PanicAlert("SaveRecord()");
	//Console::Print("SaveRecord()");
}


// Check if Dolphin is in focus
bool IsFocus()
{
#ifdef _WIN32
	HWND Parent = GetParent(g_PADInitialize.hWnd);
	HWND TopLevel = GetParent(Parent);
	// Support both rendering to main window and not
	if (GetForegroundWindow() == TopLevel || GetForegroundWindow() == g_PADInitialize.hWnd)
		return true;
	else
		return false;
#else
	return true;
#endif
}

// Implement circular deadzone
const float kDeadZone = 0.1f;
void ScaleStickValues(unsigned char* outx,
					  unsigned char* outy,
					  short inx, short iny)
{
	float x = ((float)inx + 0.5f) / 32767.5f;
	float y = ((float)iny + 0.5f) / 32767.5f;

	if ((x == 0.0f) && (y == 0.0f)) // to be safe
	{
		*outx = 0;
		*outy = 0;
		return;
	}

	float magnitude = sqrtf(x * x + y * y);
	float nx = x / magnitude;
	float ny = y / magnitude;

	if (magnitude < kDeadZone){magnitude = kDeadZone;}

	magnitude  = (magnitude - kDeadZone) / (1.0f - kDeadZone);
	magnitude *= magnitude; // another power may be more appropriate
	nx *= magnitude;
	ny *= magnitude;
	int ix = (int)(nx * 100);
	int iy = (int)(ny * 100);
	*outx = 0x80 + ix;
	*outy = 0x80 + iy;
}

// for same displacement should be sqrt(2)/2 (in theory)
// 3/4 = 0.75 is a little faster than sqrt(2)/2 = 0.7071...
// In SMS, 17/20 = 0.85 is perfect; in WW, 7/10 = 0.70 is closer.
#define DIAGONAL_SCALE 0.70710678
void EmulateAnalogStick(unsigned char *stickX, unsigned char *stickY, bool buttonUp, bool buttonDown, bool buttonLeft, bool buttonRight, int magnitude) {
	int mainY = 0;
	int mainX = 0;
	if      (buttonUp)
		mainY = magnitude;
	else if (buttonDown)
		mainY = -magnitude;
	if      (buttonLeft)
		mainX = -magnitude;
	else if (buttonRight)
		mainX = magnitude;
	// only update if there is some action
	// this allows analog stick to still work
	// disable for now, enable later if any platform supports both methods of input
	//if ((mainX != 0) && (mainY != 0)) {
	if (true) {
		if ((mainX == 0) || (mainY == 0))
		{
			*stickX += mainX;
			*stickY += mainY;
		}
		else
		{
			*stickX += mainX*DIAGONAL_SCALE;
			*stickY += mainY*DIAGONAL_SCALE;
		}
	}
}

//******************************************************************************
// Input
//******************************************************************************


#ifdef _WIN32
void DInput_Read(int _numPAD, SPADStatus* _pPADStatus)
{
	dinput.Read();

	// Analog stick values based on semi-press keys
	int mainstickvalue = (dinput.diks[pad[_numPAD].keyForControl[CTL_MAIN_SEMI]] & 0xFF) ? pad[_numPAD].Main_stick_semivalue : STICK_FULL;
	int substickvalue = (dinput.diks[pad[_numPAD].keyForControl[CTL_SUB_SEMI]] & 0xFF) ? pad[_numPAD].Sub_stick_semivalue : STICK_FULL;
	// Buttons (A/B/X/Y/Z/Start)
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_A]] & 0xFF)
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = BUTTON_FULL;
	}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_B]] & 0xFF)
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = BUTTON_FULL;
	}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_X]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_X;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_Y]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_Y;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_Z]] & 0xFF){_pPADStatus->button |= PAD_TRIGGER_Z;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_START]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_START;}
	// Triggers (L/R)
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_L]] & 0xFF)
	{
		_pPADStatus->button |= PAD_TRIGGER_L;
		_pPADStatus->triggerLeft = TRIGGER_FULL;
	}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_R]] & 0xFF)
	{
		_pPADStatus->button |= PAD_TRIGGER_R;
		_pPADStatus->triggerRight = TRIGGER_FULL;
	}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_L_SEMI]] & 0xFF)
	{
		_pPADStatus->triggerLeft = pad[_numPAD].Trigger_semivalue;
		if (pad[_numPAD].Trigger_semivalue > TRIGGER_THRESHOLD)
			_pPADStatus->button |= PAD_TRIGGER_L;
	}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_R_SEMI]] & 0xFF)
	{
		_pPADStatus->triggerRight = pad[_numPAD].Trigger_semivalue;
		if (pad[_numPAD].Trigger_semivalue > TRIGGER_THRESHOLD)
			_pPADStatus->button |= PAD_TRIGGER_R;
	}
	// Main stick
	EmulateAnalogStick(
		&_pPADStatus->stickX,
		&_pPADStatus->stickY,
		(dinput.diks[pad[_numPAD].keyForControl[CTL_MAINUP]] & 0xFF),
		(dinput.diks[pad[_numPAD].keyForControl[CTL_MAINDOWN]] & 0xFF),
		(dinput.diks[pad[_numPAD].keyForControl[CTL_MAINLEFT]] & 0xFF),
		(dinput.diks[pad[_numPAD].keyForControl[CTL_MAINRIGHT]] & 0xFF),
		mainstickvalue );
		
	// Sub-stick (C-stick)
	EmulateAnalogStick(
		&_pPADStatus->substickX,
		&_pPADStatus->substickY,
		(dinput.diks[pad[_numPAD].keyForControl[CTL_SUBUP]] & 0xFF),
		(dinput.diks[pad[_numPAD].keyForControl[CTL_SUBDOWN]] & 0xFF),
		(dinput.diks[pad[_numPAD].keyForControl[CTL_SUBLEFT]] & 0xFF),
		(dinput.diks[pad[_numPAD].keyForControl[CTL_SUBRIGHT]] & 0xFF),
		substickvalue );
	// D-pad
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_DPADUP]]    & 0xFF){_pPADStatus->button |= PAD_BUTTON_UP;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_DPADDOWN]]  & 0xFF){_pPADStatus->button |= PAD_BUTTON_DOWN;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_DPADLEFT]]  & 0xFF){_pPADStatus->button |= PAD_BUTTON_LEFT;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_DPADRIGHT]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_RIGHT;}
	// Mic key
	_pPADStatus->MicButton = (dinput.diks[pad[_numPAD].keyForControl[CTL_MIC]] & 0xFF) ? true : false;
}

bool XInput_Read(int XPadPlayer, SPADStatus* _pPADStatus)
{
	const int base = 0x80;
	XINPUT_STATE xstate;
	DWORD xresult = XInputGetState(XPadPlayer, &xstate);

	// Let's .. yes, let's use XINPUT!
	if (xresult == ERROR_SUCCESS)
	{
		const XINPUT_GAMEPAD& xpad = xstate.Gamepad;

		if ((_pPADStatus->stickX == base) && (_pPADStatus->stickY == base))
		{
			ScaleStickValues(
					&_pPADStatus->stickX,
					&_pPADStatus->stickY,
					xpad.sThumbLX,
					xpad.sThumbLY);
		}

		if ((_pPADStatus->substickX == base) && (_pPADStatus->substickY == base))
		{
			ScaleStickValues(
					&_pPADStatus->substickX,
					&_pPADStatus->substickY,
					xpad.sThumbRX,
					xpad.sThumbRY);
		}

		if (xpad.wButtons & XINPUT_GAMEPAD_DPAD_UP)       {_pPADStatus->button |= PAD_BUTTON_UP;}
		if (xpad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)     {_pPADStatus->button |= PAD_BUTTON_DOWN;}
		if (xpad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)     {_pPADStatus->button |= PAD_BUTTON_LEFT;}
		if (xpad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)    {_pPADStatus->button |= PAD_BUTTON_RIGHT;}

		_pPADStatus->triggerLeft  = xpad.bLeftTrigger;
		_pPADStatus->triggerRight = xpad.bRightTrigger;
		if (xpad.bLeftTrigger > TRIGGER_THRESHOLD)        {_pPADStatus->button |= PAD_TRIGGER_L;}
		if (xpad.bRightTrigger > TRIGGER_THRESHOLD)       {_pPADStatus->button |= PAD_TRIGGER_R;}
		if (xpad.wButtons & XINPUT_GAMEPAD_START)         {_pPADStatus->button |= PAD_BUTTON_START;}
		if (xpad.wButtons & XINPUT_GAMEPAD_A)             {_pPADStatus->button |= PAD_BUTTON_A;}
		if (xpad.wButtons & XINPUT_GAMEPAD_B)             {_pPADStatus->button |= PAD_BUTTON_X;}
		if (xpad.wButtons & XINPUT_GAMEPAD_X)             {_pPADStatus->button |= PAD_BUTTON_B;}
		if (xpad.wButtons & XINPUT_GAMEPAD_Y)             {_pPADStatus->button |= PAD_BUTTON_Y;}
		if (xpad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER){_pPADStatus->button |= PAD_TRIGGER_Z;}

		//_pPADStatus->MicButton = (xpad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? true : false;

		return true;
	}
	else
	{
		return false;
	}
}
#endif

#if (defined(HAVE_X11) && HAVE_X11) || (defined(HAVE_COCOA) && HAVE_COCOA)
#if defined(HAVE_X11) && HAVE_X11
// The graphics plugin in the PCSX2 design leaves a lot of the window processing to the pad plugin, weirdly enough.
void X11_Read(int _numPAD, SPADStatus* _pPADStatus)
{
	// Do all the stuff we need to do once per frame here
	if (_numPAD != 0)
		return;
	// This code is from Zerofrog's pcsx2 pad plugin
	XEvent E;
	//int keyPress=0, keyRelease=0;
	KeySym key;

	// keyboard input
	int num_events;
	for (num_events = XPending(GXdsp);num_events > 0;num_events--)
	{
		XNextEvent(GXdsp, &E);
		switch (E.type)
		{
		case KeyPress:
			//_KeyPress(pad, XLookupKeysym((XKeyEvent *)&E, 0));
			//break;
			key = XLookupKeysym((XKeyEvent*)&E, 0);

			if((key >= XK_F1 && key <= XK_F9) ||
				key == XK_Shift_L || key == XK_Shift_R ||
				key == XK_Control_L || key == XK_Control_R)
			{
				XPutBackEvent(GXdsp, &E);
				break;
			}
			int i;
			for (i = 0; i < NUMCONTROLS; i++) {
				if (key == pad[_numPAD].keyForControl[i]) {
					KeyStatus[i] = true;
					break;
				}
			}
			break;
		case KeyRelease:
			key = XLookupKeysym((XKeyEvent*)&E, 0);
			if((key >= XK_F1 && key <= XK_F9) ||
				key == XK_Shift_L || key == XK_Shift_R ||
				key == XK_Control_L || key == XK_Control_R)
			{
				XPutBackEvent(GXdsp, &E);
				break;
			}
			//_KeyRelease(pad, XLookupKeysym((XKeyEvent *)&E, 0));
			for (i = 0; i < NUMCONTROLS; i++)
			{
				if (key == pad[_numPAD].keyForControl[i])
				{
					KeyStatus[i] = false;
					break;
				}
			}
			break;
		}
	}
#elif defined(HAVE_COCOA) && HAVE_COCOA
void cocoa_Read(int _numPAD, SPADStatus* _pPADStatus)
{
	// Do all the stuff we need to do once per frame here
	if (_numPAD != 0)
		return;
	//get event from main thread
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSConnection *conn;
	NSDistantObject *proxy;

	conn = [NSConnection connectionWithRegisteredName:@"DolphinCocoaEvent" host:nil];
	//if (!conn) {
		//printf("error creating cnx event client\n");
	//}
	proxy = [conn rootProxy];
	//if (!proxy) {
	//	printf("error prox client\n");
	//}

	long cocoaKey = (long)[proxy keyCode];

	int i;
	if ((long)[proxy type] == 10)
	{
		for (i = 0; i < NUMCONTROLS; i++)
		{
			if (cocoaKey == pad[_numPAD].keyForControl[i])
			{
				KeyStatus[i] = true;
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < NUMCONTROLS; i++)
		{
			if (cocoaKey == pad[_numPAD].keyForControl[i])
			{
				KeyStatus[i] = false;
				break;
			}
		}
	}
#endif
	// Analog stick values based on semi-press keys
	int mainstickvalue = (KeyStatus[CTL_MAIN_SEMI]) ? pad[_numPAD].Main_stick_semivalue : STICK_FULL;
	int substickvalue = (KeyStatus[CTL_SUB_SEMI]) ? pad[_numPAD].Sub_stick_semivalue : STICK_FULL;
	// Buttons (A/B/X/Y/Z/Start)
	if (KeyStatus[CTL_A])
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = BUTTON_FULL;
	}
	if (KeyStatus[CTL_B])
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = BUTTON_FULL;
	}
	if (KeyStatus[CTL_X]){_pPADStatus->button |= PAD_BUTTON_X;}
	if (KeyStatus[CTL_Y]){_pPADStatus->button |= PAD_BUTTON_Y;}
	if (KeyStatus[CTL_Z]){_pPADStatus->button |= PAD_TRIGGER_Z;}
	if (KeyStatus[CTL_START]){_pPADStatus->button |= PAD_BUTTON_START;}
	// Triggers (L/R)
	if (KeyStatus[CTL_L]){_pPADStatus->triggerLeft = TRIGGER_FULL;}
	if (KeyStatus[CTL_R]){_pPADStatus->triggerRight = TRIGGER_FULL;}
	if (KeyStatus[CTL_L_SEMI]){_pPADStatus->triggerLeft = pad[_numPAD].Trigger_semivalue;}
	if (KeyStatus[CTL_R_SEMI]){_pPADStatus->triggerRight = pad[_numPAD].Trigger_semivalue;}
	// Main stick
	EmulateAnalogStick(
		&_pPADStatus->stickX,
		&_pPADStatus->stickY,
		KeyStatus[CTL_MAINUP],
		KeyStatus[CTL_MAINDOWN],
		KeyStatus[CTL_MAINLEFT],
		KeyStatus[CTL_MAINRIGHT],
		mainstickvalue );
	EmulateAnalogStick(
		&_pPADStatus->substickX,
		&_pPADStatus->substickY,
		KeyStatus[CTL_SUBUP],
		KeyStatus[CTL_SUBDOWN],
		KeyStatus[CTL_SUBLEFT],
		KeyStatus[CTL_SUBRIGHT],
		substickvalue );
	// D-pad
	if (KeyStatus[CTL_DPADUP])   {_pPADStatus->button |= PAD_BUTTON_UP;}
	if (KeyStatus[CTL_DPADDOWN]) {_pPADStatus->button |= PAD_BUTTON_DOWN;}
	if (KeyStatus[CTL_DPADLEFT]) {_pPADStatus->button |= PAD_BUTTON_LEFT;}
	if (KeyStatus[CTL_DPADRIGHT]){_pPADStatus->button |= PAD_BUTTON_RIGHT;}
	// Mic key
	_pPADStatus->MicButton = KeyStatus[CTL_MIC];
#if defined(HAVE_X11) && HAVE_X11
}
#elif defined(HAVE_COCOA) && HAVE_COCOA
	[pool release];
}
#endif
#endif

//******************************************************************************
// Plugin specification functions
//******************************************************************************

void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

#ifdef DEBUGFAST
	sprintf(_PluginInfo->Name, "Dolphin KB/X360pad (DebugFast)");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin KB/X360pad");
#else
	sprintf(_PluginInfo->Name, "Dolphin KB/X360pad (Debug)");
#endif
#endif

}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager *)globals->logManager);
}

void DllConfig(HWND _hParent)
{
	// Load configuration
	LoadConfig();

	// Show wxDialog
#if defined(HAVE_WX) && HAVE_WX
	if (!m_ConfigFrame)
		m_ConfigFrame = new PADConfigDialogSimple(GetParentedWxWindow(_hParent));
	else if (!m_ConfigFrame->GetParent()->IsShown())
		m_ConfigFrame->Close(true);

	// Only allow one open at a time
	if (!m_ConfigFrame->IsShown())
		m_ConfigFrame->ShowModal();
	else
		m_ConfigFrame->Hide();
#endif

	// Save configuration
	SaveConfig();
}

void DllDebugger(HWND _hParent, bool Show) {}

void Initialize(void *init)
{
	// We are now running a game
	g_EmulatorRunning = true;

	// Load configuration
	LoadConfig();

#ifdef RERECORDING
	/* Check if we are starting the pad to record the input, and an old file exists. In that case ask
	   if we really want to start the recording and eventually overwrite the file */
	if (pad[0].bRecording && File::Exists("pad-record.bin"))
	{
		if (!AskYesNo("An old version of '%s' aleady exists in your Dolphin directory. You can"
			" now make a copy of it before you start a new recording and overwrite the file."
			" Select Yes to continue and overwrite the file. Select No to turn off the input"
			" recording and continue without recording anything.",
			"pad-record.bin"))
		{
			// Turn off recording and continue
			pad[0].bRecording = false;
		}
	}

	// Load recorded input if we are to play it back, otherwise begin with a blank recording
	if (pad[0].bPlayback) LoadRecord();
#endif

	g_PADInitialize = *(SPADInitialize*)init;

	#ifdef _WIN32
		dinput.Init((HWND)g_PADInitialize.hWnd);
	#elif defined(HAVE_X11) && HAVE_X11
		GXdsp = (Display*)g_PADInitialize.hWnd;
	#elif defined(HAVE_COCOA) && HAVE_COCOA

	#endif
}

void DoState(unsigned char **ptr, int mode)
{
#ifdef RERECORDING
	// Load or save the counter
	PointerWrap p(ptr, mode);
	p.Do(count);

	//Console::Print("count: %i\n", count);

	// Update the frame counter for the sake of the status bar
	if (mode == PointerWrap::MODE_READ)
	{
		#ifdef _WIN32
			// This only works when rendering to the main window, I think
			PostMessage(GetParent(g_PADInitialize.hWnd), WM_USER, INPUT_FRAME_COUNTER, count);
		#endif
	}
#endif
}

void Shutdown()
{
	// Save the recording and reset the counter
#ifdef RERECORDING
	// Save recording
	if (pad[0].bRecording) SaveRecord();
	// Reset the counter
	count = 0;
#endif

	// We have stopped the game
	g_EmulatorRunning = false;

#ifdef _WIN32
	dinput.Free();
	// Kill xpad rumble
	XINPUT_VIBRATION vib;
	vib.wLeftMotorSpeed  = 0;
	vib.wRightMotorSpeed = 0;
	for (int i = 0; i < 4; i++)
		if (pad[i].bRumble)
			XInputSetState(pad[i].XPadPlayer, &vib);
#endif
	SaveConfig();
}


// Set buttons status from wxWidgets in the main application
void PAD_Input(u16 _Key, u8 _UpDown) {}


void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	// Check if all is okay
	if (_pPADStatus == NULL) return;

	// Play back input instead of accepting any user input
#ifdef RERECORDING
	if (pad[0].bPlayback)
	{
		*_pPADStatus = PlayRecord();
		return;
	}
#endif

	const int base = 0x80;
	// Clear pad
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	_pPADStatus->stickY = base;
	_pPADStatus->stickX = base;
	_pPADStatus->substickX = base;
	_pPADStatus->substickY = base;
	_pPADStatus->button |= PAD_USE_ORIGIN;
#ifdef _WIN32
	// Only update pad on focus, don't do this when recording
	if (pad[_numPAD].bDisable && !pad[0].bRecording && !IsFocus()) return;

	// Dolphin doesn't really care about the pad error codes anyways...
	_pPADStatus->err = PAD_ERR_NONE;

	// Read XInput
	if (pad[_numPAD].bEnableXPad)
		XInput_Read(pad[_numPAD].XPadPlayer, _pPADStatus);

	// Read Direct Input
	DInput_Read(_numPAD, _pPADStatus);

#elif defined(HAVE_X11) && HAVE_X11
	_pPADStatus->err = PAD_ERR_NONE;
	X11_Read(_numPAD, _pPADStatus);
#elif defined(HAVE_COCOA) && HAVE_COCOA
	_pPADStatus->err = PAD_ERR_NONE;
	cocoa_Read(_numPAD, _pPADStatus);
#endif

#ifdef RERECORDING
	// Record input
	if (pad[0].bRecording) RecordInput(*_pPADStatus);
#endif
}


// Rough approximation of GC behaviour - needs improvement.
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
#ifdef _WIN32
	if (pad[_numPAD].bEnableXPad)
	{
		static int a = 0;

		if ((_uType == 0) || (_uType == 2))
		{
			a = 0;
		}
		else if (_uType == 1)
		{
			a = _uStrength > 2 ? pad[_numPAD].RumbleStrength : 0;
		}

		a = int ((float)a * 0.96f);

		if (!pad[_numPAD].bRumble)
		{
			a = 0;
		}

		XINPUT_VIBRATION vib;
		vib.wLeftMotorSpeed  = a; //_uStrength*100;
		vib.wRightMotorSpeed = a; //_uStrength*100;
		XInputSetState(pad[_numPAD].XPadPlayer, &vib);
	}
#endif
}

//******************************************************************************
// Load and save the configuration
//******************************************************************************

void LoadConfig()
{
	// Initialize first pad to standard controls
#ifdef _WIN32
	const int defaultKeyForControl[NUMCONTROLS] =
	{
		DIK_X, // A
		DIK_Z, // B
		DIK_C, // X
		DIK_S, // Y
		DIK_D, // Z
		DIK_RETURN, // Start
		DIK_Q, // L
		DIK_W, // R
		0x00, // L semi-press
		0x00, // R semi-press
		DIK_UP, // Main stick up
		DIK_DOWN, // Main stick down
		DIK_LEFT, // Main stick left
		DIK_RIGHT, // Main stick right
		DIK_LSHIFT, // Main stick semi-press
		DIK_I, // C-stick up
		DIK_K, // C-stick down
		DIK_J, // C-stick left
		DIK_L, // C-stick right
		DIK_LCONTROL, // C-stick semi-press
		DIK_T, // D-pad up
		DIK_G, // D-pad down
		DIK_F, // D-pad left
		DIK_H, // D-pad right
		DIK_M, // Mic
	};
#elif defined(HAVE_X11) && HAVE_X11
	const int defaultKeyForControl[NUMCONTROLS] =
	{
		XK_x, // A
		XK_z, // B
		XK_c, // X
		XK_s, // Y
		XK_d, // Z
		XK_Return, // Start
		XK_q, // L
		XK_w, // R
		0x00, // L semi-press
		0x00, // R semi-press
		XK_Up, // Main stick up
		XK_Down, // Main stick down
		XK_Left, // Main stick left
		XK_Right, // Main stick right
		XK_Shift_L, // Main stick semi-press
		XK_i, // C-stick up
		XK_k, // C-stick down
		XK_j, // C-stick left
		XK_l, // C-stick right
		XK_Control_L, // C-stick semi-press
		XK_t, // D-pad up
		XK_g, // D-pad down
		XK_f, // D-pad left
		XK_h, // D-pad right
		XK_m, // Mic
	};
#elif defined(HAVE_COCOA) && HAVE_COCOA
	// Reference for Cocoa key codes:
	// http://boredzo.org/blog/archives/2007-05-22/virtual-key-codes
	const int defaultKeyForControl[NUMCONTROLS] =
	{
		7, // A (x)
		6, // B (z)
		8, // X (c)
		1, // Y (s)
		2, // Z (d)
		36, // Start (return)
		12, // L (q)
		13, // R (w)
		-1, // L semi-press (none)
		-1, // R semi-press (none)
		126, // Main stick up (up)
		125, // Main stick down (down)
		123, // Main stick left (left)
		124, // Main stick right (right)
		56, // Main stick semi-press (left shift)
		34, // C-stick up (i)
		40, // C-stick down (k)
		38, // C-stick left (j)
		37, // C-stick right (l)
		59, // C-stick semi-press (left control)
		17, // D-pad up (t)
		5, // D-pad down (g)
		3, // D-pad left (f)
		4, // D-pad right (h)
		46, // Mic (m)
	};
#endif

	IniFile file;
	file.Load(FULL_CONFIG_DIR "pad.ini");

	for(int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		file.Get(SectionName, "UseXPad", &pad[i].bEnableXPad, i==0);
		file.Get(SectionName, "DisableOnBackground", &pad[i].bDisable, false);
		file.Get(SectionName, "Rumble", &pad[i].bRumble, true);
		file.Get(SectionName, "RumbleStrength", &pad[i].RumbleStrength, 8000);
		file.Get(SectionName, "XPad#", &pad[i].XPadPlayer);

		file.Get(SectionName, "Trigger_semivalue", &pad[i].Trigger_semivalue, TRIGGER_HALF_DEFAULT);
		file.Get(SectionName, "Main_stick_semivalue", &pad[i].Main_stick_semivalue, STICK_HALF_DEFAULT);
		file.Get(SectionName, "Sub_stick_semivalue", &pad[i].Sub_stick_semivalue, STICK_HALF_DEFAULT);

		#ifdef RERECORDING
			file.Get(SectionName, "Recording", &pad[0].bRecording, false);
			file.Get(SectionName, "Playback", &pad[0].bPlayback, false);
		#endif

		for (int x = 0; x < NUMCONTROLS; x++)
		{
			file.Get(SectionName, controlNames[x],
			         &pad[i].keyForControl[x],
                     (i==0) ? defaultKeyForControl[x] : 0);
#if defined(HAVE_X11) && HAVE_X11
			// In linux we have a problem assigning the upper case of the
			// keys because they're not being recognized
			pad[i].keyForControl[x] = tolower(pad[i].keyForControl[x]);
#endif
		}
	}
}


void SaveConfig()
{
	IniFile file;
	file.Load(FULL_CONFIG_DIR "pad.ini");

	for(int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		file.Set(SectionName, "UseXPad", pad[i].bEnableXPad);
		file.Set(SectionName, "DisableOnBackground", pad[i].bDisable);
		file.Set(SectionName, "Rumble", pad[i].bRumble);
		file.Set(SectionName, "RumbleStrength", pad[i].RumbleStrength);
		file.Set(SectionName, "XPad#", pad[i].XPadPlayer);

		file.Set(SectionName, "Trigger_semivalue", pad[i].Trigger_semivalue);
		file.Set(SectionName, "Main_stick_semivalue", pad[i].Main_stick_semivalue);
		file.Set(SectionName, "Sub_stick_semivalue", pad[i].Sub_stick_semivalue);

		#ifdef RERECORDING
			file.Set(SectionName, "Recording", pad[0].bRecording);
			file.Set(SectionName, "Playback", pad[0].bPlayback);
		#endif

		for (int x = 0; x < NUMCONTROLS; x++)
		{
			file.Set(SectionName, controlNames[x], pad[i].keyForControl[x]);
		}
	}
	file.Save(FULL_CONFIG_DIR "pad.ini");
}
