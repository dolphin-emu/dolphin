//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003-2008 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////


////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯
#include "nJoy.h"


//////////////////////////////////////////////////////////////////////////////////////////
// Variables
// ¯¯¯¯¯¯¯¯¯

// Rumble in windows
#define _CONTROLLER_STATE_H // avoid certain declarations in nJoy.h
FILE *pFile;
HINSTANCE nJoy_hInst = NULL;
CONTROLLER_INFO	*joyinfo = 0;
CONTROLLER_STATE joystate[4];
CONTROLLER_MAPPING joysticks[4];
bool emulator_running = FALSE;
HWND m_hWnd; // Handle to window

// TODO: fix this dirty hack to stop missing symbols
void __Log(int log, const char *format, ...) {}
void __Logv(int log, int v, const char *format, ...) {}

// Rumble
#ifdef _WIN32

#elif defined(__linux__)
	extern int fd;
#endif


//////////////////////////////////////////////////////////////////////////////////////////
// wxWidgets
// ¯¯¯¯¯¯¯¯¯
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


//////////////////////////////////////////////////////////////////////////////////////////
// DllMain 
// ¯¯¯¯¯¯¯
#ifdef _WIN32
BOOL APIENTRY DllMain(	HINSTANCE hinstDLL,	// DLL module handle
						DWORD dwReason,		// reason called
						LPVOID lpvReserved)	// reserved
{	
	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
		{       
			//use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);

			if (!wxTheApp || !wxTheApp->CallOnInit() )
				return FALSE;
		}
		break; 

		case DLL_PROCESS_DETACH:		
			wxEntryCleanup(); //use wxUninitialize() if you don't want GUI 
		break;

		default:
			break;
	}
	
	nJoy_hInst = hinstDLL;	
	return TRUE;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Input Plugin Functions (from spec's)
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Get properties of plugin
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

#ifdef DEBUGFAST 
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION" (DebugFast) by Falcon4ever");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION " by Falcon4ever");
#else
	sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION" (Debug) by Falcon4ever");
#endif
#endif
}

// Call config dialog
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DllConfig(HWND _hParent)
{		
	#ifdef _WIN32
	if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
	{
		MessageBox(NULL, SDL_GetError(), "Could not initialize SDL!", MB_ICONERROR);
		return;
	}

	LoadConfig();	// load settings

	wxWindow win;
	win.SetHWND(_hParent);
	ConfigBox frame(&win);
	frame.ShowModal();
	win.SetHWND(0);

	#else
	if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
	{
		printf("Could not initialize SDL! (%s)\n", SDL_GetError());
		return;
	}

	LoadConfig();	// load settings

	#if defined(HAVE_WX) && HAVE_WX
		ConfigBox frame(NULL);
		frame.ShowModal();
	#endif

	#endif	
}

void DllDebugger(HWND _hParent, bool Show) {
}

// Init PAD (start emulation)
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Initialize(SPADInitialize _PADInitialize)
{	
	emulator_running = TRUE;
	#ifdef _DEBUG
	DEBUG_INIT();
	#endif
	
	if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
	{
		#ifdef _WIN32
		MessageBox(NULL, SDL_GetError(), "Could not initialize SDL!", MB_ICONERROR);
		#else	
		printf("Could not initialize SDL! (%s)\n", SDL_GetError());	
		#endif
		return;
	}
	
	#ifdef _WIN32
	m_hWnd = (HWND)_PADInitialize.hWnd;	
	#endif

	LoadConfig();	// Load joystick mapping
	Search_Devices();
	if (joysticks[0].enabled)
		joystate[0].joy = SDL_JoystickOpen(joysticks[0].ID);
	if (joysticks[1].enabled)
		joystate[1].joy = SDL_JoystickOpen(joysticks[1].ID);
	if (joysticks[2].enabled)
		joystate[2].joy = SDL_JoystickOpen(joysticks[2].ID);
	if (joysticks[3].enabled)
		joystate[3].joy = SDL_JoystickOpen(joysticks[3].ID);
}

// Shutdown PAD (stop emulation)
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Shutdown()
{
	if (joysticks[0].enabled)
		SDL_JoystickClose(joystate[0].joy);
	if (joysticks[1].enabled)
		SDL_JoystickClose(joystate[1].joy);
	if (joysticks[2].enabled)
		SDL_JoystickClose(joystate[2].joy);
	if (joysticks[3].enabled)
		SDL_JoystickClose(joystate[3].joy);

	SDL_Quit();
	
	#ifdef _DEBUG
		DEBUG_QUIT();
	#endif

	delete [] joyinfo;	

	emulator_running = FALSE;
	
	#ifdef _WIN32
		#ifdef USE_RUMBLE_DINPUT_HACK
		FreeDirectInput();
		#endif
	#elif defined(__linux__)
		close(fd);
	#endif
}


// Set PAD status
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	if (!joysticks[_numPAD].enabled)
		return;
		
	// Clear pad status
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	// Get pad status
	GetJoyState(_numPAD);
	
	// Reset!
	int base = 0x80;
	_pPADStatus->stickY = base;
	_pPADStatus->stickX = base;
	_pPADStatus->substickX = base;
	_pPADStatus->substickY = base;
	_pPADStatus->button |= PAD_USE_ORIGIN;

	// Set analog controllers
	// Set Deadzones perhaps out of function
	int deadzone = (int)(((float)(128.00/100.00)) * (float)(joysticks[_numPAD].deadzone+1));
	int deadzone2 = (int)(((float)(-128.00/100.00)) * (float)(joysticks[_numPAD].deadzone+1));

	// Adjust range
	// The value returned by SDL_JoystickGetAxis is a signed integer (-32768 to 32768)
	// The value used for the gamecube controller is an unsigned char (0 to 255)
	int main_stick_x = (joystate[_numPAD].axis[CTL_MAIN_X] >> 8);
	int main_stick_y = -(joystate[_numPAD].axis[CTL_MAIN_Y] >> 8);
    int sub_stick_x = (joystate[_numPAD].axis[CTL_SUB_X] >> 8);
	int sub_stick_y = -(joystate[_numPAD].axis[CTL_SUB_Y] >> 8);

	// Quick fix
	if (main_stick_x > 127)
		main_stick_x = 127;
	if (main_stick_y > 127)
		main_stick_y = 127;
	if (sub_stick_x > 127)
		sub_stick_x = 127;
	if (sub_stick_y > 127)
		sub_stick_y = 127;

	if (main_stick_x < -128)
		main_stick_x = -128;
	if (main_stick_y < -128)
		main_stick_y = -128;
	if (sub_stick_x < -128)
		sub_stick_x = -128;
	if (sub_stick_y < -128)
		sub_stick_y = -128;

	// Send values to Dolpin
	if ((main_stick_x < deadzone2)	|| (main_stick_x > deadzone))	_pPADStatus->stickX += main_stick_x;
	if ((main_stick_y < deadzone2)	|| (main_stick_y > deadzone))	_pPADStatus->stickY += main_stick_y;
	if ((sub_stick_x < deadzone2)	|| (sub_stick_x > deadzone))	_pPADStatus->substickX += sub_stick_x;
	if ((sub_stick_y < deadzone2)	|| (sub_stick_y > deadzone))	_pPADStatus->substickY += sub_stick_y;

	int triggervalue = 255;
	if (joystate[_numPAD].halfpress)
		triggervalue = 100;

	// Set buttons
	if (joystate[_numPAD].buttons[CTL_L_SHOULDER])
	{
		_pPADStatus->button|=PAD_TRIGGER_L;
		_pPADStatus->triggerLeft  = triggervalue;
	}
	if (joystate[_numPAD].buttons[CTL_R_SHOULDER])	
	{
		_pPADStatus->button|=PAD_TRIGGER_R;
		_pPADStatus->triggerRight = triggervalue;
	}

	if (joystate[_numPAD].buttons[CTL_A_BUTTON])
	{
		_pPADStatus->button|=PAD_BUTTON_A;
		_pPADStatus->analogA = 255;			// Perhaps support pressure?
	}
	if (joystate[_numPAD].buttons[CTL_B_BUTTON])
	{
		_pPADStatus->button|=PAD_BUTTON_B;
		_pPADStatus->analogB = 255;			// Perhaps support pressure?
	}
	if (joystate[_numPAD].buttons[CTL_X_BUTTON])	_pPADStatus->button|=PAD_BUTTON_X;
	if (joystate[_numPAD].buttons[CTL_Y_BUTTON])	_pPADStatus->button|=PAD_BUTTON_Y;
	if (joystate[_numPAD].buttons[CTL_Z_TRIGGER])	_pPADStatus->button|=PAD_TRIGGER_Z;
	if (joystate[_numPAD].buttons[CTL_START])		_pPADStatus->button|=PAD_BUTTON_START;

	// Set D-pad
	if (joysticks[_numPAD].controllertype == CTL_TYPE_JOYSTICK)
	{
		if (joystate[_numPAD].dpad == SDL_HAT_LEFTUP	|| joystate[_numPAD].dpad == SDL_HAT_UP		|| joystate[_numPAD].dpad == SDL_HAT_RIGHTUP )		_pPADStatus->button|=PAD_BUTTON_UP;
		if (joystate[_numPAD].dpad == SDL_HAT_LEFTUP	|| joystate[_numPAD].dpad == SDL_HAT_LEFT	|| joystate[_numPAD].dpad == SDL_HAT_LEFTDOWN )		_pPADStatus->button|=PAD_BUTTON_LEFT;
		if (joystate[_numPAD].dpad == SDL_HAT_LEFTDOWN	|| joystate[_numPAD].dpad == SDL_HAT_DOWN	|| joystate[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_DOWN;
		if (joystate[_numPAD].dpad == SDL_HAT_RIGHTUP	|| joystate[_numPAD].dpad == SDL_HAT_RIGHT	|| joystate[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_RIGHT;
	}
	else
	{
		if (joystate[_numPAD].dpad2[CTL_D_PAD_UP])
			_pPADStatus->button|=PAD_BUTTON_UP;
		if (joystate[_numPAD].dpad2[CTL_D_PAD_DOWN])
			_pPADStatus->button|=PAD_BUTTON_DOWN;
		if (joystate[_numPAD].dpad2[CTL_D_PAD_LEFT])
			_pPADStatus->button|=PAD_BUTTON_LEFT;
		if (joystate[_numPAD].dpad2[CTL_D_PAD_RIGHT])
			_pPADStatus->button|=PAD_BUTTON_RIGHT;
	}
	
	//
	_pPADStatus->err = PAD_ERR_NONE;

	// Use rumble
	PAD_Use_Rumble(_numPAD, _pPADStatus);
}




// Set PAD attached pads
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;
	
	LoadConfig();

	if (joysticks[0].enabled)
		connected |= 1;		
	if (joysticks[1].enabled)
		connected |= 2;
	if (joysticks[2].enabled)
		connected |= 4;
	if (joysticks[3].enabled)
		connected |= 8;

	return connected;
}



//////////////////////////////////////////////////////////////////////////////////////////
// Custom Functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Read buttons status. Called from GetJoyState().
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ReadButton(int controller, int button)
{
	int ctl_button = joysticks[controller].buttons[button];
	if (ctl_button < joyinfo[joysticks[controller].ID].NumButtons)
	{
		joystate[controller].buttons[button] = SDL_JoystickGetButton(joystate[controller].joy, ctl_button);
	}
}

// Request joystick state
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void GetJoyState(int controller)
{
	SDL_JoystickUpdate();

	joystate[controller].axis[CTL_MAIN_X] = SDL_JoystickGetAxis(joystate[controller].joy, joysticks[controller].axis[CTL_MAIN_X]);
	joystate[controller].axis[CTL_MAIN_Y] = SDL_JoystickGetAxis(joystate[controller].joy, joysticks[controller].axis[CTL_MAIN_Y]);
	joystate[controller].axis[CTL_SUB_X] = SDL_JoystickGetAxis(joystate[controller].joy, joysticks[controller].axis[CTL_SUB_X]);
	joystate[controller].axis[CTL_SUB_Y] = SDL_JoystickGetAxis(joystate[controller].joy, joysticks[controller].axis[CTL_SUB_Y]);
	
	ReadButton(controller, CTL_L_SHOULDER);
	ReadButton(controller, CTL_R_SHOULDER);
	ReadButton(controller, CTL_A_BUTTON);
	ReadButton(controller, CTL_B_BUTTON);
	ReadButton(controller, CTL_X_BUTTON);
	ReadButton(controller, CTL_Y_BUTTON);
	ReadButton(controller, CTL_Z_TRIGGER);
	ReadButton(controller, CTL_START);

	PanicAlert("%i", CTL_A_BUTTON);
	
	//
	if (joysticks[controller].halfpress < joyinfo[controller].NumButtons)
		joystate[controller].halfpress = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].halfpress);

	// Check if we have an analog or digital joypad
	if (joysticks[controller].controllertype == CTL_TYPE_JOYSTICK)
	{
		joystate[controller].dpad = SDL_JoystickGetHat(joystate[controller].joy, joysticks[controller].dpad);
	}
	else
	{
		joystate[controller].dpad2[CTL_D_PAD_UP] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].dpad2[CTL_D_PAD_UP]);
		joystate[controller].dpad2[CTL_D_PAD_DOWN] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].dpad2[CTL_D_PAD_DOWN]);
		joystate[controller].dpad2[CTL_D_PAD_LEFT] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].dpad2[CTL_D_PAD_LEFT]);
		joystate[controller].dpad2[CTL_D_PAD_RIGHT] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].dpad2[CTL_D_PAD_RIGHT]);
	}
}

// Search attached devices
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
int Search_Devices()
{
	// load config
	#ifdef _DEBUG
	DEBUG_INIT();
	#endif

	int numjoy = SDL_NumJoysticks();

	if (joyinfo)
	{
		delete [] joyinfo;
		joyinfo = new CONTROLLER_INFO [numjoy];
	}
	else
	{
		joyinfo = new CONTROLLER_INFO [numjoy];
	}

	// Warn the user of no joysticks are detected
	if (numjoy == 0)
	{		
		#ifdef _WIN32
		MessageBox(NULL, "No Joystick detected!", NULL,  MB_ICONWARNING);
		#else	
		printf("No Joystick detected!\n");	
		#endif
		return 0;
	}

	#ifdef _DEBUG
	fprintf(pFile, "Scanning for devices\n");
	fprintf(pFile, "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\n");
	#endif
	
	for(int i = 0; i < numjoy; i++ )
	{
		joyinfo[i].joy			= SDL_JoystickOpen(i);
		joyinfo[i].ID			= i;
		joyinfo[i].NumAxes		= SDL_JoystickNumAxes(joyinfo[i].joy);
		joyinfo[i].NumButtons	= SDL_JoystickNumButtons(joyinfo[i].joy);
		joyinfo[i].NumBalls		= SDL_JoystickNumBalls(joyinfo[i].joy);
		joyinfo[i].NumHats		= SDL_JoystickNumHats(joyinfo[i].joy);
		joyinfo[i].Name			= SDL_JoystickName(i);
  
		#ifdef _DEBUG
		fprintf(pFile, "ID: %d\n", i);
		fprintf(pFile, "Name: %s\n", joyinfo[i].Name);
		fprintf(pFile, "Buttons: %d\n", joyinfo[i].NumButtons);
		fprintf(pFile, "Axes: %d\n", joyinfo[i].NumAxes);
		fprintf(pFile, "Hats: %d\n", joyinfo[i].NumHats);
		fprintf(pFile, "Balls: %d\n\n", joyinfo[i].NumBalls);
		#endif	

		// Close if opened
		if (SDL_JoystickOpened(i))
			SDL_JoystickClose(joyinfo[i].joy);
	}

	return numjoy;
}
