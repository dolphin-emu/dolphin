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
#include <wx/aboutdlg.h>

#include "Common.h"
#include "pluginspecs_pad.h"
#include "PadSimple.h"
#include "IniFile.h"

#include "GUI/ConfigDlg.h"

#ifdef XINPUT_ENABLE
#include "XInput.h"
#endif

#ifdef _WIN32

#include "DirectInputBase.h"

DInput dinput;

#else

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

Display* GXdsp;
bool KeyStatus[23];
#endif

SPads pad[4];

HINSTANCE g_hInstance;
SPADInitialize g_PADInitialize;

#define RECORD_SIZE (1024 * 128)
SPADStatus recordBuffer[RECORD_SIZE];
int count = 0;

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

class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp)

WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);

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
	sprintf(_PluginInfo->Name, "Dolphin KB/X360pad");
#else
	sprintf(_PluginInfo->Name, "Dolphin KB/X360pad (Debug)");
#endif
#endif

}


void DllAbout(HWND _hParent)
{
	wxAboutDialogInfo info;
	info.AddDeveloper(_T("ector"));
	info.AddDeveloper(_T("F|RES"));
	info.SetDescription(_T("Simple keyboard and XInput plugin for dolphin"));
	wxAboutBox(info);
}


void DllConfig(HWND _hParent)
{
	LoadConfig();
#ifdef _WIN32
	wxWindow win;
	win.SetHWND(_hParent);
	ConfigDialog frame(&win);
	frame.ShowModal();
	win.SetHWND(0);
#else
	ConfigDialog frame(NULL);
	frame.ShowModal();
#endif
	SaveConfig();
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
	GXdsp = (Display*)g_PADInitialize.hWnd;
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
	/*We can do all now :D
	if (_numPad != 0)
	{
		return;
	}*/

	dinput.Read();

	int mainvalue =    (dinput.diks[pad[_numPad].keyForControl[CTL_HALFMAIN]]   & 0xFF) ? 40 : 100;
	int subvalue =     (dinput.diks[pad[_numPad].keyForControl[CTL_HALFSUB]]    & 0xFF) ? 40 : 100;
	int triggervalueL = (dinput.diks[pad[_numPad].keyForControl[CTL_TRIGGER_L]] & 0xFF) ? 100 : 255;
	int triggervalueR = (dinput.diks[pad[_numPad].keyForControl[CTL_TRIGGER_R]] & 0xFF) ? 100 : 255;

	// get the new keys
	if (dinput.diks[pad[_numPad].keyForControl[CTL_MAINLEFT]] & 0xFF){_pPADStatus->stickX -= mainvalue;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_MAINRIGHT]] & 0xFF){_pPADStatus->stickX += mainvalue;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_MAINDOWN]] & 0xFF){_pPADStatus->stickY -= mainvalue;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_MAINUP]]   & 0xFF){_pPADStatus->stickY += mainvalue;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_SUBLEFT]]  & 0xFF){_pPADStatus->substickX -= subvalue;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_SUBRIGHT]] & 0xFF){_pPADStatus->substickX += subvalue;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_SUBDOWN]]  & 0xFF){_pPADStatus->substickY -= subvalue;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_SUBUP]]    & 0xFF){_pPADStatus->substickY += subvalue;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_L]] & 0xFF)
	{
		_pPADStatus->button |= PAD_TRIGGER_L;
		_pPADStatus->triggerLeft = triggervalueL;
	}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_R]] & 0xFF)
	{
		_pPADStatus->button |= PAD_TRIGGER_R;
		_pPADStatus->triggerRight = triggervalueR;
	}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_A]] & 0xFF)
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = 255;
	}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_B]] & 0xFF)
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = 255;
	}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_X]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_X;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_Y]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_Y;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_Z]] & 0xFF){_pPADStatus->button |= PAD_TRIGGER_Z;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_DPADUP]]   & 0xFF){_pPADStatus->button |= PAD_BUTTON_UP;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_DPADDOWN]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_DOWN;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_DPADLEFT]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_LEFT;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_DPADRIGHT]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_RIGHT;}

	if (dinput.diks[pad[_numPad].keyForControl[CTL_START]]    & 0xFF){_pPADStatus->button |= PAD_BUTTON_START;}
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

#if defined(__linux__)
// The graphics plugin in the PCSX2 design leaves a lot of the window processing to the pad plugin, weirdly enough.
void X11_Read(int _numPAD, SPADStatus* _pPADStatus)
{
    // Do all the stuff we need to do once per frame here
    if (_numPAD != 0)
	{
            return;
	}
    int i;
    // This code is from Zerofrog's pcsx2 pad plugin
    XEvent E;
    //int keyPress=0, keyRelease=0;
    KeySym key;

    // keyboard input
    while (XPending(GXdsp) > 0) {

        XNextEvent(GXdsp, &E);

        switch (E.type) {
          case KeyPress:
              //_KeyPress(pad, XLookupKeysym((XKeyEvent *)&E, 0)); break;
              key = XLookupKeysym((XKeyEvent*)&E, 0);
              
              for (i = 0; i < NUMCONTROLS; i++) {
                  if (key == pad[_numPAD].keyForControl[i]) {
                      KeyStatus[i] = true;
                      break;
                  }
              }
              break;
              
          case KeyRelease:
              key = XLookupKeysym((XKeyEvent*)&E, 0);
              
              //_KeyRelease(pad, XLookupKeysym((XKeyEvent *)&E, 0));
              for (i = 0; i < NUMCONTROLS; i++) {
                  if (key == pad[_numPAD].keyForControl[i]) {
                      KeyStatus[i] = false;
                      break;
                  }
              }
              break;
              
          case FocusIn:
              XAutoRepeatOff(GXdsp);
              break;
              
          case FocusOut:
              XAutoRepeatOn(GXdsp);
              break;
        }
    }
    
    int mainvalue =    (KeyStatus[20]) ? 40 : 100;
    int subvalue =     (KeyStatus[21]) ? 40 : 100;
    int triggervalue = (KeyStatus[23]) ? 100 : 255;
	
    if (KeyStatus[0]){_pPADStatus->stickX -= mainvalue;}
    if (KeyStatus[1]){_pPADStatus->stickY += mainvalue;}
    if (KeyStatus[2]){_pPADStatus->stickX += mainvalue;}
    if (KeyStatus[3]){_pPADStatus->stickY -= mainvalue;}
	
    if (KeyStatus[4]){_pPADStatus->substickX -= subvalue;}
    if (KeyStatus[5]){_pPADStatus->substickY += subvalue;}
    if (KeyStatus[6]){_pPADStatus->substickX += subvalue;}
    if (KeyStatus[7]){_pPADStatus->substickY -= subvalue;}
	
    if (KeyStatus[8]){_pPADStatus->button |= PAD_BUTTON_LEFT;}
    if (KeyStatus[9]){_pPADStatus->button |= PAD_BUTTON_UP;}
    if (KeyStatus[10]){_pPADStatus->button |= PAD_BUTTON_RIGHT;}
    if (KeyStatus[11]){_pPADStatus->button |= PAD_BUTTON_DOWN;}

    if (KeyStatus[12]) {
        _pPADStatus->button |= PAD_BUTTON_A;
        _pPADStatus->analogA = 255;
    }
    
    if (KeyStatus[13]) {
        _pPADStatus->button |= PAD_BUTTON_B;
        _pPADStatus->analogB = 255;
    }

    if (KeyStatus[14]){_pPADStatus->button |= PAD_BUTTON_X;}
    if (KeyStatus[15]){_pPADStatus->button |= PAD_BUTTON_Y;}
    if (KeyStatus[16]){_pPADStatus->button |= PAD_TRIGGER_Z;}
	
    if (KeyStatus[17]) {
        _pPADStatus->button |= PAD_TRIGGER_L;
        _pPADStatus->triggerLeft = triggervalue;
    }

    if (KeyStatus[18]) {
        _pPADStatus->button |= PAD_TRIGGER_R;
        _pPADStatus->triggerRight = triggervalue;
    }

    if (KeyStatus[19]){_pPADStatus->button |= PAD_BUTTON_START;}
}


#endif

void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
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
#elif defined(__linux__)
	X11_Read(_numPAD, _pPADStatus);
#endif

#ifdef RECORD_STORE
	RecordInput(*_pPADStatus);
#endif
}


// Rough approximation of GC behaviour - needs improvement.
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
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

	if (!pad[_numPAD].rumble)
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

unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;
	
	LoadConfig();

	if(pad[0].attached)
		connected |= 1;		
	if(pad[1].attached)
		connected |= 2;
	if(pad[2].attached)
		connected |= 4;
	if(pad[3].attached)
		connected |= 8;

	return connected;
}


unsigned int SaveLoadState(char* _ptr, BOOL _bSave)
{
	return(0);
}


void LoadConfig()
{
	// Initialize pads to standard controls
#ifdef _WIN32	
	const int defaultKeyForControl[NUMCONTROLS] =
	{
		DIK_X,	//A
		DIK_Z,
		DIK_S,
		DIK_C,
		DIK_D,
		DIK_RETURN,
		DIK_Q,
		DIK_LCONTROL,
		DIK_W,
		DIK_LCONTROL,
		DIK_UP,	//mainstick
		DIK_DOWN,
		DIK_LEFT,
		DIK_RIGHT,
		DIK_LSHIFT,
		DIK_I,	//substick
		DIK_K,
		DIK_J,
		DIK_L,
		DIK_LSHIFT,
		DIK_T,	//dpad
		DIK_G,
		DIK_F,
		DIK_H
	};
#else
	const int defaultKeyForControl[NUMCONTROLS] =
	{
          XK_x, //A
          XK_z,
          XK_s,
          XK_c,
          XK_d,
          XK_Return,
          XK_q,
          XK_Control_L,
          XK_w,
          XK_Control_L,
          XK_Up, //mainstick
          XK_Down,
          XK_Left, 
          XK_Right,
          XK_Shift_L,
          XK_i, //substick
          XK_K,
          XK_j,
          XK_l,
          XK_Shift_L,
          XK_t, //dpad
          XK_g,
          XK_f,
          XK_h
	};
#endif
	IniFile file;
	file.Load("pad.ini");

	for(int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		if(pad[i].type == 0)
		{
			//keyboard settings
			file.Get(SectionName, "Type", &pad[i].type);
			file.Get(SectionName, "Attached", &pad[i].attached, false);
			file.Get(SectionName, "DisableOnBackground", &pad[i].disable, false);
			
			for (int x = 0; x < NUMCONTROLS; x++)
			{
				file.Get(SectionName, controlNames[x], &pad[i].keyForControl[x], defaultKeyForControl[x]);
			}
		}
		else
		{
			//xpad settings
			file.Get(SectionName, "Type", &pad[i].type);
			file.Get(SectionName, "Attached", &pad[i].attached, false);
			file.Get(SectionName, "DisableOnBackground", &pad[i].disable, false);
			file.Get(SectionName, "Rumble", &pad[i].rumble, true);
			file.Get(SectionName, "XPad#", &pad[i].XPad);
		}
	}
}


void SaveConfig()
{
	IniFile file;
	file.Load("pad.ini");

	for(int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		if(pad[i].type == 0)
		{
			//keyboard settings
			file.Set(SectionName, "Type", pad[i].type);
			file.Set(SectionName, "Attached", pad[i].attached);
			file.Set(SectionName, "DisableOnBackground", pad[i].disable);
			
			for (int x = 0; x < NUMCONTROLS; x++)
			{
				file.Set(SectionName, controlNames[x], pad[i].keyForControl[x]);
			}
		}
		else
		{
			//xpad settings
			file.Set(SectionName, "Type", pad[i].type);
			file.Set(SectionName, "Attached", pad[i].attached);
			file.Set(SectionName, "DisableOnBackground", pad[i].disable);
			file.Set(SectionName, "Rumble", pad[i].rumble);
			file.Set(SectionName, "XPad#", pad[i].XPad);
		}
	}
	file.Save("pad.ini");
}


