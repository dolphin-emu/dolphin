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

#include <stdio.h>
#include <math.h>

#include "Common.h"
#include "pluginspecs_pad.h"
#include "PadSimple.h"
#include "IniFile.h"

#if !defined(OSX64)
#include "GUI/ConfigDlg.h"
#endif

#ifdef _WIN32
#include "XInput.h"
#include "DirectInputBase.h"

DInput dinput;

#else

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

Display* GXdsp;
bool KeyStatus[NUMCONTROLS];
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

void DllConfig(HWND _hParent)
{
	LoadConfig();
#ifdef _WIN32
	wxWindow win;
	win.SetHWND(_hParent);
	ConfigDialog frame(&win);
	frame.ShowModal();
	win.SetHWND(0);
#elif !defined(OSX64)
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
void DInput_Read(int _numPAD, SPADStatus* _pPADStatus)
{
	dinput.Read();

	int stickvalue =    (dinput.diks[pad[_numPAD].keyForControl[CTL_HALFPRESS]] & 0xFF) ? 40 : 100;
	int triggervalue = (dinput.diks[pad[_numPAD].keyForControl[CTL_HALFPRESS]] & 0xFF) ? 100 : 255;
	
	// get the new keys
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_MAINLEFT]] & 0xFF){_pPADStatus->stickX -= stickvalue;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_MAINRIGHT]] & 0xFF){_pPADStatus->stickX += stickvalue;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_MAINDOWN]] & 0xFF){_pPADStatus->stickY -= stickvalue;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_MAINUP]]   & 0xFF){_pPADStatus->stickY += stickvalue;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_SUBLEFT]]  & 0xFF){_pPADStatus->substickX -= stickvalue;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_SUBRIGHT]] & 0xFF){_pPADStatus->substickX += stickvalue;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_SUBDOWN]]  & 0xFF){_pPADStatus->substickY -= stickvalue;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_SUBUP]]    & 0xFF){_pPADStatus->substickY += stickvalue;}

	if (dinput.diks[pad[_numPAD].keyForControl[CTL_L]] & 0xFF)
	{
		_pPADStatus->button |= PAD_TRIGGER_L;
		_pPADStatus->triggerLeft = triggervalue;
	}

	if (dinput.diks[pad[_numPAD].keyForControl[CTL_R]] & 0xFF)
	{
		_pPADStatus->button |= PAD_TRIGGER_R;
		_pPADStatus->triggerRight = triggervalue;
	}

	if (dinput.diks[pad[_numPAD].keyForControl[CTL_A]] & 0xFF)
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = 255;
	}

	if (dinput.diks[pad[_numPAD].keyForControl[CTL_B]] & 0xFF)
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = 255;
	}

	if (dinput.diks[pad[_numPAD].keyForControl[CTL_X]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_X;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_Y]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_Y;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_Z]] & 0xFF){_pPADStatus->button |= PAD_TRIGGER_Z;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_DPADUP]]   & 0xFF){_pPADStatus->button |= PAD_BUTTON_UP;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_DPADDOWN]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_DOWN;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_DPADLEFT]] & 0xFF){_pPADStatus->button |= PAD_BUTTON_LEFT;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_DPADRIGHT]]& 0xFF){_pPADStatus->button |= PAD_BUTTON_RIGHT;}
	if (dinput.diks[pad[_numPAD].keyForControl[CTL_START]]    & 0xFF){_pPADStatus->button |= PAD_BUTTON_START;}
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

		_pPADStatus->triggerLeft  = xpad.bLeftTrigger;
		_pPADStatus->triggerRight = xpad.bRightTrigger;

		if (xpad.bLeftTrigger > 200)					{_pPADStatus->button |= PAD_TRIGGER_L;}
		if (xpad.bRightTrigger > 200)					{_pPADStatus->button |= PAD_TRIGGER_R;}
		if (xpad.wButtons & XINPUT_GAMEPAD_A)			{_pPADStatus->button |= PAD_BUTTON_A;}
		if (xpad.wButtons & XINPUT_GAMEPAD_X)			{_pPADStatus->button |= PAD_BUTTON_B;}
		if (xpad.wButtons & XINPUT_GAMEPAD_B)			{_pPADStatus->button |= PAD_BUTTON_X;}
		if (xpad.wButtons & XINPUT_GAMEPAD_Y)			{_pPADStatus->button |= PAD_BUTTON_Y;}
		if (xpad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER){_pPADStatus->button |= PAD_TRIGGER_Z;}
		if (xpad.wButtons & XINPUT_GAMEPAD_START)		{_pPADStatus->button |= PAD_BUTTON_START;}
		if (xpad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)	{_pPADStatus->button |= PAD_BUTTON_LEFT;}
		if (xpad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)	{_pPADStatus->button |= PAD_BUTTON_RIGHT;}
		if (xpad.wButtons & XINPUT_GAMEPAD_DPAD_UP)		{_pPADStatus->button |= PAD_BUTTON_UP;}
		if (xpad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)	{_pPADStatus->button |= PAD_BUTTON_DOWN;}

		return true;
	}
	else
	{
		return false;
	}
}
#endif

#if defined(__linux__)
// The graphics plugin in the PCSX2 design leaves a lot of the window processing to the pad plugin, weirdly enough.
void X11_Read(int _numPAD, SPADStatus* _pPADStatus)
{
    // Do all the stuff we need to do once per frame here
    if (_numPAD != 0) {
        return;
    }
    // This code is from Zerofrog's pcsx2 pad plugin
    XEvent E;
    //int keyPress=0, keyRelease=0;
    KeySym key;
    
    // keyboard input
    int num_events;
    for (num_events = XPending(GXdsp);num_events > 0;num_events--) {
        XNextEvent(GXdsp, &E);
        switch (E.type) {
        case KeyPress:
            //_KeyPress(pad, XLookupKeysym((XKeyEvent *)&E, 0)); break;

            key = XLookupKeysym((XKeyEvent*)&E, 0);
            
            if((key >= XK_F1 && key <= XK_F9) || 
               key == XK_Shift_L || key == XK_Shift_R ||
               key == XK_Control_L || key == XK_Control_R) {
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
               key == XK_Control_L || key == XK_Control_R) {
                XPutBackEvent(GXdsp, &E);
                break;
            }
          
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
            
        default:
            break;
        }
    }

    int stickvalue =   (KeyStatus[CTL_HALFPRESS]) ? 40 : 100;
    int triggervalue = (KeyStatus[CTL_HALFPRESS]) ? 100 : 255;

    if (KeyStatus[CTL_MAINLEFT]){_pPADStatus->stickX -= stickvalue;}
    if (KeyStatus[CTL_MAINUP]){_pPADStatus->stickY += stickvalue;}
    if (KeyStatus[CTL_MAINRIGHT]){_pPADStatus->stickX += stickvalue;}
    if (KeyStatus[CTL_MAINDOWN]){_pPADStatus->stickY -= stickvalue;}

    if (KeyStatus[CTL_SUBLEFT]){_pPADStatus->substickX -= stickvalue;}
    if (KeyStatus[CTL_SUBUP]){_pPADStatus->substickY += stickvalue;}
    if (KeyStatus[CTL_SUBRIGHT]){_pPADStatus->substickX += stickvalue;}
    if (KeyStatus[CTL_SUBDOWN]){_pPADStatus->substickY -= stickvalue;}

    if (KeyStatus[CTL_DPADLEFT]){_pPADStatus->button |= PAD_BUTTON_LEFT;}
    if (KeyStatus[CTL_DPADUP]){_pPADStatus->button |= PAD_BUTTON_UP;}
    if (KeyStatus[CTL_DPADRIGHT]){_pPADStatus->button |= PAD_BUTTON_RIGHT;}
    if (KeyStatus[CTL_DPADDOWN]){_pPADStatus->button |= PAD_BUTTON_DOWN;}

    if (KeyStatus[CTL_A]) {
        _pPADStatus->button |= PAD_BUTTON_A;
        _pPADStatus->analogA = 255;
    }
    
    if (KeyStatus[CTL_B]) {
        _pPADStatus->button |= PAD_BUTTON_B;
        _pPADStatus->analogB = 255;
    }
    
    if (KeyStatus[CTL_X]){_pPADStatus->button |= PAD_BUTTON_X;}
    if (KeyStatus[CTL_Y]){_pPADStatus->button |= PAD_BUTTON_Y;}
    if (KeyStatus[CTL_Z]){_pPADStatus->button |= PAD_TRIGGER_Z;}
	
    if (KeyStatus[CTL_L]) {
        _pPADStatus->button |= PAD_TRIGGER_L;
        _pPADStatus->triggerLeft = triggervalue;
    }
    
    if (KeyStatus[CTL_R]) {
        _pPADStatus->button |= PAD_TRIGGER_R;
        _pPADStatus->triggerRight = triggervalue;
    }
    
    if (KeyStatus[CTL_START]){_pPADStatus->button |= PAD_BUTTON_START;}
    if (KeyStatus[CTL_MIC])
    	_pPADStatus->MicButton = true;
    else
    	_pPADStatus->MicButton = false;
}


#endif

void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	// Check if all is okay
	if ((_pPADStatus == NULL))
	{
		return;
	}

#ifdef RECORD_REPLAY
	*_pPADStatus = PlayRecord();
	return;
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
	// Just update pad on focus
	// TODO fix g_PADInitialize.hWnd != DolphinWX frame
	if (pad[_numPAD].bDisable)
	{
		if (g_PADInitialize.hWnd != GetForegroundWindow())
			return;
	}
	// Dolphin doesn't really care about the pad error codes anyways...
	_pPADStatus->err = PAD_ERR_NONE;
	if (pad[_numPAD].bEnableXPad) XInput_Read(pad[_numPAD].XPadPlayer, _pPADStatus);
	DInput_Read(_numPAD, _pPADStatus);
#elif defined(__linux__)
	_pPADStatus->err = PAD_ERR_NONE;
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
	if (pad[_numPAD].bEnableXPad)
	{
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

unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;
	
	LoadConfig();

	if(pad[0].bAttached)
		connected |= 1;		
	if(pad[1].bAttached)
		connected |= 2;
	if(pad[2].bAttached)
		connected |= 4;
	if(pad[3].bAttached)
		connected |= 8;

	return connected;
}


unsigned int SaveLoadState(char* _ptr, BOOL _bSave)
{
	return(0);
}


void LoadConfig()
{
	// Initialize first pad to standard controls
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
		DIK_W,
		DIK_UP,	//mainstick
		DIK_DOWN,
		DIK_LEFT,
		DIK_RIGHT,
		DIK_I,	//substick
		DIK_K,
		DIK_J,
		DIK_L,
		DIK_T,	//dpad
		DIK_G,
		DIK_F,
		DIK_H,
		DIK_LSHIFT
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
          XK_w,
          XK_Up, //mainstick
          XK_Down,
          XK_Left, 
          XK_Right,
          XK_i, //substick
          XK_K,
          XK_j,
          XK_l,
          XK_t, //dpad
          XK_g,
          XK_f,
          XK_h,
		  XK_Shift_L, //halfpress
		  XK_p
	};
#endif
	IniFile file;
	file.Load(FULL_CONFIG_DIR "pad.ini");

	for(int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		file.Get(SectionName, "UseXPad", &pad[i].bEnableXPad, i==0);
		file.Get(SectionName, "Attached", &pad[i].bAttached, i==0);
		file.Get(SectionName, "DisableOnBackground", &pad[i].bDisable, false);
		file.Get(SectionName, "Rumble", &pad[i].bRumble, true);
		file.Get(SectionName, "XPad#", &pad[i].XPadPlayer);
		
		for (int x = 0; x < NUMCONTROLS; x++) 
		{
			file.Get(SectionName, controlNames[x], &pad[i].keyForControl[x], 
                                     (i==0)?defaultKeyForControl[x]:0);
#ifndef _WIN32
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
		file.Set(SectionName, "Attached", pad[i].bAttached);
		file.Set(SectionName, "DisableOnBackground", pad[i].bDisable);
		file.Set(SectionName, "Rumble", pad[i].bRumble);
		file.Set(SectionName, "XPad#", pad[i].XPadPlayer);
		
		for (int x = 0; x < NUMCONTROLS; x++)
		{
			file.Set(SectionName, controlNames[x], pad[i].keyForControl[x]);
		}
	}
	file.Save(FULL_CONFIG_DIR "pad.ini");
}


