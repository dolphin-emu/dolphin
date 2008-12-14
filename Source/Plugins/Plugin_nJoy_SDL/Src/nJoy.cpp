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

#include "nJoy.h"

//////////////////////////////////////////////////////////////////////////////////////////
// Variables
// ¯¯¯¯¯¯¯¯¯

FILE *pFile;
HINSTANCE nJoy_hInst = NULL;
CONTROLLER_INFO	*joyinfo = 0;
CONTROLLER_STATE joystate[4];
CONTROLLER_MAPPING joysticks[4];
bool emulator_running = FALSE;

// Handle to window
HWND m_hWnd;

#ifdef USE_RUMBLE_DINPUT_HACK
bool g_rumbleEnable = FALSE;
#endif

// Rumble in windows
#ifdef _WIN32

#ifdef USE_RUMBLE_DINPUT_HACK
LPDIRECTINPUT8          g_pDI = NULL;
LPDIRECTINPUTDEVICE8    g_pDevice = NULL;
LPDIRECTINPUTEFFECT     g_pEffect = NULL;

DWORD                   g_dwNumForceFeedbackAxis = 0;
INT                     g_nXForce = 0;
INT                     g_nYForce = 0;

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }

HRESULT InitDirectInput(HWND hDlg);
VOID FreeDirectInput();
BOOL CALLBACK EnumFFDevicesCallback(const DIDEVICEINSTANCE* pInst, VOID* pContext);
BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext);
HRESULT SetDeviceForcesXY();
#endif

#elif defined(__linux__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

	int fd;
	char device_file_name[64];
	struct ff_effect effect;
	bool CanRumble = false;
#endif

#if defined(HAVE_WX) && HAVE_WX
//////////////////////////////////////////////////////////////////////////////////////////
// wxWidgets
// ¯¯¯¯¯¯¯¯¯
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
		
	// clear pad status
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	// get pad status
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
	int main_stick_x = (joystate[_numPAD].axis[CTL_MAIN_X]>>8);
	int main_stick_y = -(joystate[_numPAD].axis[CTL_MAIN_Y]>>8);
    int sub_stick_x = (joystate[_numPAD].axis[CTL_SUB_X]>>8);
	int sub_stick_y = -(joystate[_numPAD].axis[CTL_SUB_Y]>>8);

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
		if (joystate[_numPAD].dpad == SDL_HAT_LEFTUP		|| joystate[_numPAD].dpad == SDL_HAT_UP		|| joystate[_numPAD].dpad == SDL_HAT_RIGHTUP )		_pPADStatus->button|=PAD_BUTTON_UP;
		if (joystate[_numPAD].dpad == SDL_HAT_LEFTUP		|| joystate[_numPAD].dpad == SDL_HAT_LEFT	|| joystate[_numPAD].dpad == SDL_HAT_LEFTDOWN )		_pPADStatus->button|=PAD_BUTTON_LEFT;
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
	
	_pPADStatus->err = PAD_ERR_NONE;


	#ifdef _WIN32
	#ifdef USE_RUMBLE_DINPUT_HACK
	if (joystate[_numPAD].halfpress)
	if (!g_pDI)
	if (FAILED(InitDirectInput(m_hWnd)))
	{
		MessageBox(NULL, SDL_GetError(), "Could not initialize DirectInput!", MB_ICONERROR);
		g_rumbleEnable = FALSE;
		//return;
	}
	else
		g_rumbleEnable = TRUE;

	if (g_rumbleEnable)
	{
		g_pDevice->Acquire();
		
		if (g_pEffect)
			g_pEffect->Start(1, 0);
	}
	#endif
	#elif defined(__linux__)
	if (!fd)
	{
		sprintf(device_file_name, "/dev/input/event%d", joysticks[_numPAD].eventnum); //TODO: Make dynamic //

		/* Open device */
		fd = open(device_file_name, O_RDWR);
		if (fd == -1) {
			perror("Open device file");
			//Something wrong, probably permissions, just return now
			return;
		}
		int n_effects = 0;
		if (ioctl(fd, EVIOCGEFFECTS, &n_effects) == -1) {
			perror("Ioctl number of effects");
		}
		if (n_effects > 0)
			CanRumble = true;
		else
			return; // Return since we can't do any effects
		/* a strong rumbling effect */
		effect.type = FF_RUMBLE;
		effect.id = -1;
		effect.u.rumble.strong_magnitude = 0x8000;
		effect.u.rumble.weak_magnitude = 0;
		effect.replay.length = 5000; // Set to 5 seconds, if a Game needs more for a single rumble event, it is dumb and must be a demo
		effect.replay.delay = 0;
		if (ioctl(fd, EVIOCSFF, &effect) == -1) {
			perror("Upload effect");
			CanRumble = false; //We have effects but it doesn't support the rumble we are using. This is basic rumble, should work for most
		}
	}
	#endif
}

// Set PAD rumble
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// (Stop=0, Rumble=1)
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	//if (_numPAD > 0)
	//	return;

	// not supported by SDL
	// So we need to use platform specific stuff
	#ifdef _WIN32
	#ifdef USE_RUMBLE_DINPUT_HACK
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
	else
	{
		g_nYForce = a;
		SetDeviceForcesXY();
	}
	#endif
	#elif defined(__linux__)
	struct input_event event;
	if (CanRumble)
	{
		if (_uType == 1)
		{
			event.type = EV_FF;
			event.code = effect.id;
			event.value = 1;
			if (write(fd, (const void*) &event, sizeof(event)) == -1) {
				perror("Play effect");
				exit(1);
			}
		}
		if ((_uType == 0) || (_uType == 2))
		{
			event.type = EV_FF;
			event.code =  effect.id;
			event.value = 0;
			if (write(fd, (const void*) &event, sizeof(event)) == -1) {
				perror("Stop effect");
				exit(1);
			}
		}
	}
	#endif
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

void ReadButton(int controller, int button) {
	int ctl_button = joysticks[controller].buttons[button];
	if (ctl_button < joyinfo[joysticks[controller].ID].NumButtons) {
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
	
	if (joysticks[controller].halfpress < joyinfo[controller].NumButtons)
		joystate[controller].halfpress = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].halfpress);

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

	if (numjoy == 0)
	{		
		#ifdef _WIN32
		MessageBox(NULL, "No Joystick detected!", NULL,  MB_ICONWARNING);
		#else	
		printf("No Joystick detected!\n");	
		#endif
		return 0;
	}

	if (joyinfo)
	{
		delete [] joyinfo;
		joyinfo = new CONTROLLER_INFO [numjoy];
	}
	else
	{
		joyinfo = new CONTROLLER_INFO [numjoy];
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

// Enable output log
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DEBUG_INIT()
{
	if (pFile)
		return;

	#ifdef _WIN32
	char dateStr [9]; 
	_strdate( dateStr);
	char timeStr [9]; 
	_strtime( timeStr );
	#endif

	pFile = fopen ("nJoy-debug.txt","wt");
	fprintf(pFile, "nJoy v"INPUT_VERSION" Debug\n");
	#ifdef _WIN32
	fprintf(pFile, "Date: %s\nTime: %s\n", dateStr, timeStr);
	#endif
	fprintf(pFile, "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\n");
}

// Disable output log
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DEBUG_QUIT()
{
	if (!pFile)
		return;

	#ifdef _WIN32
	char timeStr [9]; 
	_strtime(timeStr);

	fprintf(pFile, "_______________\n");
	fprintf(pFile, "Time: %s", timeStr);
	#endif
	fclose(pFile);
}

// Save settings to file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void SaveConfig()
{
	IniFile file;
	file.Load("nJoy.ini");

	for (int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		file.Set(SectionName, "l_shoulder", joysticks[i].buttons[CTL_L_SHOULDER]);
		file.Set(SectionName, "r_shoulder", joysticks[i].buttons[CTL_R_SHOULDER]);
		file.Set(SectionName, "a_button", joysticks[i].buttons[CTL_A_BUTTON]);
		file.Set(SectionName, "b_button", joysticks[i].buttons[CTL_B_BUTTON]);
		file.Set(SectionName, "x_button", joysticks[i].buttons[CTL_X_BUTTON]);
		file.Set(SectionName, "y_button", joysticks[i].buttons[CTL_Y_BUTTON]);
		file.Set(SectionName, "z_trigger", joysticks[i].buttons[CTL_Z_TRIGGER]);
		file.Set(SectionName, "start_button", joysticks[i].buttons[CTL_START]);
		file.Set(SectionName, "dpad", joysticks[i].dpad);	
		file.Set(SectionName, "dpad_up", joysticks[i].dpad2[CTL_D_PAD_UP]);
		file.Set(SectionName, "dpad_down", joysticks[i].dpad2[CTL_D_PAD_DOWN]);
		file.Set(SectionName, "dpad_left", joysticks[i].dpad2[CTL_D_PAD_LEFT]);
		file.Set(SectionName, "dpad_right", joysticks[i].dpad2[CTL_D_PAD_RIGHT]);
		file.Set(SectionName, "main_x", joysticks[i].axis[CTL_MAIN_X]);
		file.Set(SectionName, "main_y", joysticks[i].axis[CTL_MAIN_Y]);
		file.Set(SectionName, "sub_x", joysticks[i].axis[CTL_SUB_X]);
		file.Set(SectionName, "sub_y", joysticks[i].axis[CTL_SUB_Y]);
		file.Set(SectionName, "enabled", joysticks[i].enabled);
		file.Set(SectionName, "deadzone", joysticks[i].deadzone);
		file.Set(SectionName, "halfpress", joysticks[i].halfpress);
		file.Set(SectionName, "joy_id", joysticks[i].ID);
		file.Set(SectionName, "controllertype", joysticks[i].controllertype);
		file.Set(SectionName, "eventnum", joysticks[i].eventnum);
	}

	file.Save("nJoy.ini");
}

// Load settings from file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void LoadConfig()
{
	IniFile file;
	file.Load("nJoy.ini");

	for (int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		file.Get(SectionName, "l_shoulder", &joysticks[i].buttons[CTL_L_SHOULDER], 4);
		file.Get(SectionName, "r_shoulder", &joysticks[i].buttons[CTL_R_SHOULDER], 5);
		file.Get(SectionName, "a_button", &joysticks[i].buttons[CTL_A_BUTTON], 0);
		file.Get(SectionName, "b_button", &joysticks[i].buttons[CTL_B_BUTTON], 1);	
		file.Get(SectionName, "x_button", &joysticks[i].buttons[CTL_X_BUTTON], 3);
		file.Get(SectionName, "y_button", &joysticks[i].buttons[CTL_Y_BUTTON], 2);	
		file.Get(SectionName, "z_trigger", &joysticks[i].buttons[CTL_Z_TRIGGER], 7);
		file.Get(SectionName, "start_button", &joysticks[i].buttons[CTL_START], 9);	
		file.Get(SectionName, "dpad", &joysticks[i].dpad, 0);	
		file.Get(SectionName, "dpad_up", &joysticks[i].dpad2[CTL_D_PAD_UP], 0);
		file.Get(SectionName, "dpad_down", &joysticks[i].dpad2[CTL_D_PAD_DOWN], 0);
		file.Get(SectionName, "dpad_left", &joysticks[i].dpad2[CTL_D_PAD_LEFT], 0);
		file.Get(SectionName, "dpad_right", &joysticks[i].dpad2[CTL_D_PAD_RIGHT], 0);
		file.Get(SectionName, "main_x", &joysticks[i].axis[CTL_MAIN_X], 0);	
		file.Get(SectionName, "main_y", &joysticks[i].axis[CTL_MAIN_Y], 1);	
		file.Get(SectionName, "sub_x", &joysticks[i].axis[CTL_SUB_X], 2);	
		file.Get(SectionName, "sub_y", &joysticks[i].axis[CTL_SUB_Y], 3);	
		file.Get(SectionName, "enabled", &joysticks[i].enabled, 1);	
		file.Get(SectionName, "deadzone", &joysticks[i].deadzone, 9);	
		file.Get(SectionName, "halfpress", &joysticks[i].halfpress, 6);	
		file.Get(SectionName, "joy_id", &joysticks[i].ID, 0);
		file.Get(SectionName, "controllertype", &joysticks[i].controllertype, 0);
		file.Get(SectionName, "eventnum", &joysticks[i].eventnum, 0);
	}
}


#ifdef _WIN32
//////////////////////////////////////////////////////////////////////////////////////////
// Rumble stuff :D!
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//
#ifdef USE_RUMBLE_DINPUT_HACK
HRESULT InitDirectInput( HWND hDlg )
{
    DIPROPDWORD dipdw;
    HRESULT hr;

    // Register with the DirectInput subsystem and get a pointer to a IDirectInput interface we can use.
    if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_pDI, NULL)))
    {
        return hr;
    }

    // Look for a force feedback device we can use
    if (FAILED(hr = g_pDI->EnumDevices( DI8DEVCLASS_GAMECTRL, EnumFFDevicesCallback, NULL, DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK)))
    {
        return hr;
    }

    if (NULL == g_pDevice)
    {
        MessageBox(NULL, "Force feedback device not found. nJoy will now disable rumble." ,"FFConst" , MB_ICONERROR | MB_OK);
		g_rumbleEnable = FALSE;
        
        return S_OK;
    }

    // Set the data format to "simple joystick" - a predefined data format. A
    // data format specifies which controls on a device we are interested in,
    // and how they should be reported.
    //
    // This tells DirectInput that we will be passing a DIJOYSTATE structure to
    // IDirectInputDevice8::GetDeviceState(). Even though we won't actually do
    // it in this sample. But setting the data format is important so that the
    // DIJOFS_* values work properly.
    if (FAILED(hr = g_pDevice->SetDataFormat(&c_dfDIJoystick)))
        return hr;

    // Set the cooperative level to let DInput know how this device should
    // interact with the system and with other DInput applications.
    // Exclusive access is required in order to perform force feedback.
    //if (FAILED(hr = g_pDevice->SetCooperativeLevel(hDlg, DISCL_EXCLUSIVE | DISCL_FOREGROUND)))

	if (FAILED(hr = g_pDevice->SetCooperativeLevel(hDlg, DISCL_EXCLUSIVE | DISCL_FOREGROUND)))	
    {
        return hr;
    }

    // Since we will be playing force feedback effects, we should disable the
    // auto-centering spring.
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = FALSE;

    if (FAILED(hr = g_pDevice->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph)))
        return hr;

    // Enumerate and count the axes of the joystick 
    if (FAILED(hr = g_pDevice->EnumObjects(EnumAxesCallback, (VOID*)&g_dwNumForceFeedbackAxis, DIDFT_AXIS)))
        return hr;

    // This simple sample only supports one or two axis joysticks
    if (g_dwNumForceFeedbackAxis > 2)
        g_dwNumForceFeedbackAxis = 2;

    // This application needs only one effect: Applying raw forces.
    DWORD rgdwAxes[2] = {DIJOFS_X, DIJOFS_Y};
    LONG rglDirection[2] = {0, 0};
    DICONSTANTFORCE cf = {0};

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwSamplePeriod = 0;
    eff.dwGain = DI_FFNOMINALMAX;
    eff.dwTriggerButton = DIEB_NOTRIGGER;
    eff.dwTriggerRepeatInterval = 0;
    eff.cAxes = g_dwNumForceFeedbackAxis;
    eff.rgdwAxes = rgdwAxes;
    eff.rglDirection = rglDirection;
    eff.lpEnvelope = 0;
    eff.cbTypeSpecificParams = sizeof( DICONSTANTFORCE );
    eff.lpvTypeSpecificParams = &cf;
    eff.dwStartDelay = 0;

    // Create the prepared effect
    if (FAILED(hr = g_pDevice->CreateEffect(GUID_ConstantForce, &eff, &g_pEffect, NULL)))
    {
        return hr;
    }

    if (NULL == g_pEffect)
        return E_FAIL;

    return S_OK;
}

VOID FreeDirectInput()
{
    // Unacquire the device one last time just in case 
    // the app tried to exit while the device is still acquired.
    if (g_pDevice)
        g_pDevice->Unacquire();

    // Release any DirectInput objects.
    SAFE_RELEASE(g_pEffect);
    SAFE_RELEASE(g_pDevice);
    SAFE_RELEASE(g_pDI);
}

BOOL CALLBACK EnumFFDevicesCallback( const DIDEVICEINSTANCE* pInst, VOID* pContext )
{
    LPDIRECTINPUTDEVICE8 pDevice;
    HRESULT hr;

    // Obtain an interface to the enumerated force feedback device.
    hr = g_pDI->CreateDevice(pInst->guidInstance, &pDevice, NULL);

    // If it failed, then we can't use this device for some bizarre reason.  
	// (Maybe the user unplugged it while we were in the middle of enumerating it.)  So continue enumerating
    if (FAILED(hr))
        return DIENUM_CONTINUE;

    // We successfully created an IDirectInputDevice8.  So stop looking for another one.
    g_pDevice = pDevice;

    return DIENUM_STOP;
}

BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext)
{
    DWORD* pdwNumForceFeedbackAxis = (DWORD*)pContext;
    if ((pdidoi->dwFlags & DIDOI_FFACTUATOR) != 0)
        (*pdwNumForceFeedbackAxis)++;

    return DIENUM_CONTINUE;
}

HRESULT SetDeviceForcesXY()
{
    // Modifying an effect is basically the same as creating a new one, except you need only specify the parameters you are modifying
    LONG rglDirection[2] = { 0, 0 };

    DICONSTANTFORCE cf;

    if (g_dwNumForceFeedbackAxis == 1)
    {
        // If only one force feedback axis, then apply only one direction and keep the direction at zero
        cf.lMagnitude = g_nXForce;
        rglDirection[0] = 0;
    }
    else
    {
        // If two force feedback axis, then apply magnitude from both directions 
        rglDirection[0] = g_nXForce;
        rglDirection[1] = g_nYForce;
        cf.lMagnitude = (DWORD)sqrt((double)g_nXForce * (double)g_nXForce + (double)g_nYForce * (double)g_nYForce );
    }

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.cAxes = g_dwNumForceFeedbackAxis;
    eff.rglDirection = rglDirection;
    eff.lpEnvelope = 0;
    eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
    eff.lpvTypeSpecificParams = &cf;
    eff.dwStartDelay = 0;

    // Now set the new parameters and start the effect immediately.
    return g_pEffect->SetParameters(&eff, DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START);
}
#endif
#endif
