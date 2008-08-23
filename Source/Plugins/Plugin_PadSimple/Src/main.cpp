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

#ifdef _WIN32
#define XINPUT_ENABLE
#endif

#include <stdio.h>
#include <math.h>

#include "Common.h"

#ifdef XINPUT_ENABLE
#include <XInput.h>
#endif

#include "pluginspecs_pad.h"

#include "IniFile.h"

#ifdef _WIN32

#include "DirectInputBase.h"
#include "resource.h"
#include "AboutDlg.h"
#include "ConfigDlg.h"

DInput dinput;

#else

#include "SDL.h"
SDL_Joystick *joy;
int numaxes = 0;
int numbuttons = 0;
int numballs = 0;

#endif

// controls
enum
{
	CTL_MAINLEFT = 0,
	CTL_MAINUP,
	CTL_MAINRIGHT,
	CTL_MAINDOWN,
	CTL_SUBLEFT,
	CTL_SUBUP,
	CTL_SUBRIGHT,
	CTL_SUBDOWN,
	CTL_DPADLEFT,
	CTL_DPADUP,
	CTL_DPADRIGHT,
	CTL_DPADDOWN,
	CTL_A,
	CTL_B,
	CTL_X,
	CTL_Y,
	CTL_Z,
	CTL_L,
	CTL_R,
	CTL_START,
	CTL_HALFMAIN,
	CTL_HALFSUB,
	CTL_HALFTRIGGER,
	NUMCONTROLS
};

// control names
static const char* controlNames[] =
{
	"Main_stick_left",
	"Main_stick_up",
	"Main_stick_right",
	"Main_stick_down",
	"Sub_stick_left",
	"Sub_stick_up",
	"Sub_stick_right",
	"Sub_stick_down",
	"D-Pad_left",
	"D-Pad_up",
	"D-Pad_right",
	"D-Pad_down",
	"A_button",
	"B_button",
	"X_button",
	"Y_button",
	"Z_trigger",
	"L_button",
	"R_button",
	"Start",
	"Soft_main_switch",
	"Soft_sub_switch",
	"Soft_triggers_switch",
};

int keyForControl[NUMCONTROLS];

HINSTANCE g_hInstance = NULL;
SPADInitialize g_PADInitialize;
bool g_rumbleEnable = true;

void LoadConfig();
void SaveConfig();


#define RECORD_SIZE (1024 * 128)
SPADStatus recordBuffer[RECORD_SIZE];
int count = 0;


// #define RECORD_STORE
// #define RECORD_REPLAY


void RecordInput(const SPADStatus& _rPADStatus)
{
	if (count >= RECORD_SIZE)
	{
		return;
	}

	recordBuffer[count++] = _rPADStatus;
}


const SPADStatus& PlayRecord()
{
	if (count >= RECORD_SIZE){return(recordBuffer[0]);}

	return(recordBuffer[count++]);
}


void LoadRecord()
{
	FILE* pStream = fopen("c:\\pad-record.bin", "rb");

	if (pStream != NULL)
	{
		fread(recordBuffer, 1, RECORD_SIZE * sizeof(SPADStatus), pStream);
		fclose(pStream);
	}
}


void SaveRecord()
{
	FILE* pStream = fopen("c:\\pad-record.bin", "wb");

	if (pStream != NULL)
	{
		fwrite(recordBuffer, 1, RECORD_SIZE * sizeof(SPADStatus), pStream);
		fclose(pStream);
	}
}


#ifdef _WIN32
BOOL APIENTRY DllMain(HINSTANCE hinstDLL, // DLL module handle
		DWORD dwReason, // reason called
		LPVOID lpvReserved) // reserved
{
	switch (dwReason)
	{
	    case DLL_PROCESS_ATTACH:
		    break;

	    case DLL_PROCESS_DETACH:
		    break;

	    default:
		    break;
	}

	g_hInstance = hinstDLL;
	return(TRUE);
}
#endif


void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

#ifdef DEBUGFAST 
	sprintf(_PluginInfo->Name, "Dolphin KB/X360pad (DebugFast)");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin KB/X360pad ");
#else
	sprintf(_PluginInfo->Name, "Dolphin KB/X360pad (Debug)");
#endif
#endif

}


void DllAbout(HWND _hParent)
{
#ifdef _WIN32
	CAboutDlg aboutDlg;
	aboutDlg.DoModal(_hParent);
#endif
}

#ifndef _WIN32
void SDL_Inputinit()
{
	if ( SDL_Init(SDL_INIT_JOYSTICK) < 0 ) 
	{
		printf("Unable to init SDL: %s\n", SDL_GetError());
		exit(1);
	}
	for(int a = 0; a < SDL_NumJoysticks();a ++)
		printf("Name:%s\n",SDL_JoystickName(a));
	// Open joystick
	joy=SDL_JoystickOpen(0);
  
  if(joy)
  {
    printf("Opened Joystick 0\n");
    printf("Name: %s\n", SDL_JoystickName(0));
    printf("Number of Axes: %d\n", SDL_JoystickNumAxes(joy));
    printf("Number of Buttons: %d\n", SDL_JoystickNumButtons(joy));
    printf("Number of Balls: %d\n", SDL_JoystickNumBalls(joy));
    numaxes = SDL_JoystickNumAxes(joy);
    numbuttons = SDL_JoystickNumButtons(joy);
    numballs = SDL_JoystickNumBalls(joy);
  }
  else
    printf("Couldn't open Joystick 0\n");
}
#endif

void DllConfig(HWND _hParent)
{
#ifdef _WIN32
	LoadConfig();

	CConfigDlg configDlg;
	configDlg.DoModal(_hParent);

	SaveConfig();
#else
	SDL_Inputinit();
#endif
}


void PAD_Initialize(SPADInitialize _PADInitialize)
{
#ifdef RECORD_REPLAY
	LoadRecord();
#endif

	g_PADInitialize = _PADInitialize;
#ifdef _WIN32
	dinput.Init((HWND)g_PADInitialize.hWnd);
#else
	SDL_Inputinit();
#endif

	LoadConfig();
}


void PAD_Shutdown()
{
#ifdef RECORD_STORE
	SaveRecord();
#endif
#ifdef _WIN32
	dinput.Free();
#else
  // Close if opened
  if(SDL_JoystickOpened(0))
    SDL_JoystickClose(joy);
#endif
	SaveConfig();
}


const float kDeadZone = 0.1f;

// Implement circular deadzone
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


#ifdef _WIN32
void DInput_Read(int _numPad, SPADStatus* _pPADStatus)
{
	if (_numPad != 0)
	{
		return;
	}

	dinput.Read();

	int mainvalue =    (dinput.diks[keyForControl[CTL_HALFMAIN]]   & 0xFF) ? 40 : 100;
	int subvalue =     (dinput.diks[keyForControl[CTL_HALFSUB]]    & 0xFF) ? 40 : 100;
	int triggervalue = (dinput.diks[keyForControl[CTL_HALFTRIGGER]] & 0xFF) ? 100 : 255;

	// get the new keys
	if (dinput.diks[keyForControl[CTL_MAINLEFT]] & 0xFF){_pPADStatus->stickX -= mainvalue;}

	if (dinput.diks[keyForControl[CTL_MAINRIGHT]] & 0xFF){_pPADStatus->stickX += mainvalue;}

	if (dinput.diks[keyForControl[CTL_MAINDOWN]] & 0xFF){_pPADStatus->stickY -= mainvalue;}

	if (dinput.diks[keyForControl[CTL_MAINUP]]   & 0xFF){_pPADStatus->stickY += mainvalue;}

	if (dinput.diks[keyForControl[CTL_SUBLEFT]]  & 0xFF){_pPADStatus->substickX -= subvalue;}

	if (dinput.diks[keyForControl[CTL_SUBRIGHT]] & 0xFF){_pPADStatus->substickX += subvalue;}

	if (dinput.diks[keyForControl[CTL_SUBDOWN]]  & 0xFF){_pPADStatus->substickY -= subvalue;}

	if (dinput.diks[keyForControl[CTL_SUBUP]]    & 0xFF){_pPADStatus->substickY += subvalue;}

	if (dinput.diks[keyForControl[CTL_L]] & 0xFF)
	{
		_pPADStatus->button |= PAD_TRIGGER_L;
		_pPADStatus->triggerLeft = triggervalue;
	}

	if (dinput.diks[keyForControl[CTL_R]] & 0xFF)
	{
		_pPADStatus->button |= PAD_TRIGGER_R;
		_pPADStatus->triggerRight = triggervalue;
	}

	if (dinput.diks[keyForControl[CTL_A]] & 0xFF)
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = 255;
	}

	if (dinput.diks[keyForControl[CTL_B]] & 0xFF)
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = 255;
	}

	if (dinput.diks[keyForControl[CTL_X]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_X;}

	if (dinput.diks[keyForControl[CTL_Y]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_Y;}

	if (dinput.diks[keyForControl[CTL_Z]] & 0xFF){_pPADStatus->button |= PAD_TRIGGER_Z;}

	if (dinput.diks[keyForControl[CTL_DPADUP]]   & 0xFF){_pPADStatus->button |= PAD_BUTTON_UP;}

	if (dinput.diks[keyForControl[CTL_DPADDOWN]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_DOWN;}

	if (dinput.diks[keyForControl[CTL_DPADLEFT]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_LEFT;}

	if (dinput.diks[keyForControl[CTL_DPADRIGHT]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_RIGHT;}

	if (dinput.diks[keyForControl[CTL_START]]    & 0xFF){_pPADStatus->button |= PAD_BUTTON_START;}
}

void XInput_Read(int _numPAD, SPADStatus* _pPADStatus)
{
#ifdef XINPUT_ENABLE
	const int base = 0x80;
	XINPUT_STATE xstate;
	DWORD xresult = XInputGetState(_numPAD, &xstate);

	if ((xresult != ERROR_SUCCESS) && (_numPAD != 0))
	{
		return;
	}

	// In addition, let's .. yes, let's use XINPUT!
	if (xresult == ERROR_SUCCESS)
	{
		const XINPUT_GAMEPAD& pad = xstate.Gamepad;

		if ((_pPADStatus->stickX == base) && (_pPADStatus->stickY == base))
		{
			ScaleStickValues(
					&_pPADStatus->stickX,
					&_pPADStatus->stickY,
					pad.sThumbLX,
					pad.sThumbLY);
		}

		if ((_pPADStatus->substickX == base) && (_pPADStatus->substickY == base))
		{
			ScaleStickValues(
					&_pPADStatus->substickX,
					&_pPADStatus->substickY,
					pad.sThumbRX,
					pad.sThumbRY);
		}

		_pPADStatus->triggerLeft  = pad.bLeftTrigger;
		_pPADStatus->triggerRight = pad.bRightTrigger;

		if (pad.bLeftTrigger > 20){_pPADStatus->button |= PAD_TRIGGER_L;}

		if (pad.bRightTrigger > 20){_pPADStatus->button |= PAD_TRIGGER_R;}

		if (pad.wButtons & XINPUT_GAMEPAD_A){_pPADStatus->button |= PAD_BUTTON_A;}

		if (pad.wButtons & XINPUT_GAMEPAD_X){_pPADStatus->button |= PAD_BUTTON_B;}

		if (pad.wButtons & XINPUT_GAMEPAD_B){_pPADStatus->button |= PAD_BUTTON_X;}

		if (pad.wButtons & XINPUT_GAMEPAD_Y){_pPADStatus->button |= PAD_BUTTON_Y;}

		if (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER){_pPADStatus->button |= PAD_TRIGGER_Z;}

		if (pad.wButtons & XINPUT_GAMEPAD_START){_pPADStatus->button |= PAD_BUTTON_START;}

		if (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT){_pPADStatus->button |= PAD_BUTTON_LEFT;}

		if (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT){_pPADStatus->button |= PAD_BUTTON_RIGHT;}

		if (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP){_pPADStatus->button |= PAD_BUTTON_UP;}

		if (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN){_pPADStatus->button |= PAD_BUTTON_DOWN;}
	}
#endif

}


#endif
int a = 0;
#ifndef _WIN32
// The graphics plugin in the PCSX2 design leaves a lot of the window processing to the pad plugin, weirdly enough.
void X11_Read(int _numPAD, SPADStatus* _pPADStatus)
{
	// Do all the stuff we need to do once per frame here
	if (_numPAD != 0)
	{
		return;
	}
	SDL_JoystickUpdate();
	for(int a = 0;a < numbuttons;a++)
	{
		if(SDL_JoystickGetButton(joy, a))
		{
			switch(a)
			{
				case 0://A
					_pPADStatus->button |= PAD_BUTTON_A;
					_pPADStatus->analogA = 255;
				break;
				case 1://B
					_pPADStatus->button |= PAD_BUTTON_B;
					_pPADStatus->analogB = 255;
				break;
				case 2://X
					_pPADStatus->button |= PAD_BUTTON_Y;
				break;
				case 3://Y
					_pPADStatus->button |= PAD_BUTTON_X;
				break;
				case 5://RB
					_pPADStatus->button |= PAD_TRIGGER_Z;
				break;
				case 6://Start
					_pPADStatus->button |= PAD_BUTTON_START;
				break;
			}
		}
	}

}


#endif
unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;
	connected |= 1;	
	return connected;
}
void PAD_GetStatus(BYTE _numPAD, SPADStatus* _pPADStatus)
{
	// check if all is okay
	if ((_pPADStatus == NULL))
	{
		return;
	}

#ifdef RECORD_REPLAY
	*_pPADStatus = PlayRecord();
	return;
#endif




	const int base = 0x80;
	// clear pad
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	_pPADStatus->stickY = base;
	_pPADStatus->stickX = base;
	_pPADStatus->substickX = base;
	_pPADStatus->substickY = base;
	_pPADStatus->button |= PAD_USE_ORIGIN;
#ifdef _WIN32
	// just update pad on focus
	//if (g_PADInitialize.hWnd != ::GetForegroundWindow())
	//	return;
#endif
	_pPADStatus->err = PAD_ERR_NONE;

	// keyboard is hardwired to player 1.
#ifdef _WIN32
	DInput_Read(_numPAD, _pPADStatus);
	XInput_Read(_numPAD, _pPADStatus);
#else
	X11_Read(_numPAD, _pPADStatus);
#endif

#ifdef RECORD_STORE
	RecordInput(*_pPADStatus);
#endif
}


// Rough approximation of GC behaviour - needs improvement.
void PAD_Rumble(BYTE _numPAD, unsigned int _uType, unsigned int _uStrength)
{
#ifdef _WIN32
	static int a = 0;

	if ((_uType == 0) || (_uType == 2))
	{
		a = 0;
	}
	else if (_uType == 1)
	{
		a = _uStrength > 2 ? 8000 : 0;
	}

	a = int ((float)a * 0.96f);

	if (!g_rumbleEnable)
	{
		a = 0;
	}

#ifdef XINPUT_ENABLE
	XINPUT_VIBRATION vib;
	vib.wLeftMotorSpeed  = a; //_uStrength*100;
	vib.wRightMotorSpeed = a; //_uStrength*100;
	XInputSetState(_numPAD, &vib);
#endif
#endif
}


unsigned int SaveLoadState(char* _ptr, BOOL _bSave)
{
	return(0);
}


void LoadConfig()
{
#ifdef _WIN32
	// Initialize pad 1 to standard controls
	const int defaultKeyForControl[NUMCONTROLS] =
	{
		DIK_LEFT, //mainstick
		DIK_UP,
		DIK_RIGHT,
		DIK_DOWN,
		DIK_J, //substick
		DIK_I,
		DIK_L,
		DIK_K,
		DIK_F, //dpad
		DIK_T,
		DIK_H,
		DIK_G,
		DIK_X, //buttons
		DIK_Z,
		DIK_S,
		DIK_C,
		DIK_D,
		DIK_Q,
		DIK_W,
		DIK_RETURN,
		DIK_LSHIFT,
		DIK_LSHIFT,
		DIK_LCONTROL
	};
#endif
	IniFile file;
	file.Load("pad.ini");

	for (int i = 0; i < NUMCONTROLS; i++)
	{
#ifdef _WIN32
		file.Get("Bindings", controlNames[i], &keyForControl[i], defaultKeyForControl[i]);
#endif
	}

	file.Get("XPad1", "Rumble", &g_rumbleEnable, true);
}


void SaveConfig()
{
	IniFile file;
	file.Load("pad.ini");

	for (int i = 0; i < NUMCONTROLS; i++)
	{
		file.Set("Bindings", controlNames[i], keyForControl[i]);
	}

	file.Set("XPad1", "Rumble", g_rumbleEnable);
	file.Save("pad.ini");
}


