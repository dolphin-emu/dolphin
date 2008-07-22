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

//////////////////////////////////////////////////////////////////////////////////////////
// DllMain 
// ¯¯¯¯¯¯¯
BOOL APIENTRY DllMain(	HINSTANCE hinstDLL,	// DLL module handle
						DWORD dwReason,		// reason called
						LPVOID lpvReserved)	// reserved
{
	InitCommonControls();

	nJoy_hInst = hinstDLL;	
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Input Plugin Functions (from spec's)
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Get properties of plugin
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

	#ifndef _DEBUG
		sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION " by Falcon4ever");
	#else
		sprintf(_PluginInfo->Name, "nJoy v"INPUT_VERSION" (Debug) by Falcon4ever");
	#endif
}

// Call about dialog
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DllAbout(HWND _hParent)
{
	OpenAbout(nJoy_hInst, _hParent);
}

// Call config dialog
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DllConfig(HWND _hParent)
{
	if(SDL_Init(SDL_INIT_JOYSTICK ) < 0)
	{
		MessageBox(NULL, SDL_GetError(), "Could not initialize SDL!", MB_ICONERROR);
		return;
	}

	LoadConfig();
	if(OpenConfig(nJoy_hInst, _hParent))
		SaveConfig();
}

// Init PAD (start emulation)
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Initialize(SPADInitialize _PADInitialize)
{	
	emulator_running = TRUE;
	#ifdef _DEBUG
	DEBUG_INIT();
	#endif
	
	if(SDL_Init(SDL_INIT_JOYSTICK ) < 0)
	{
		MessageBox(NULL, SDL_GetError(), "Could not initialize SDL!", MB_ICONERROR);
		return;
	}
	
	LoadConfig();	// Load joystick mapping
	
	if(joysticks[0].enabled)
		joystate[0].joy = SDL_JoystickOpen(joysticks[0].ID);
	if(joysticks[1].enabled)
		joystate[1].joy = SDL_JoystickOpen(joysticks[1].ID);
	if(joysticks[2].enabled)
		joystate[2].joy = SDL_JoystickOpen(joysticks[2].ID);
	if(joysticks[3].enabled)
		joystate[3].joy = SDL_JoystickOpen(joysticks[3].ID);
}

// Shutdown PAD (stop emulation)
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Shutdown()
{
	if(joysticks[0].enabled)
		SDL_JoystickClose(joystate[0].joy);
	if(joysticks[1].enabled)
		SDL_JoystickClose(joystate[1].joy);
	if(joysticks[2].enabled)
		SDL_JoystickClose(joystate[2].joy);
	if(joysticks[3].enabled)
		SDL_JoystickClose(joystate[3].joy);

	SDL_Quit();
	
	#ifdef _DEBUG
	DEBUG_QUIT();
	#endif

	delete [] joyinfo;	

	emulator_running = FALSE;
}

// Set PAD status
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_GetStatus(BYTE _numPAD, SPADStatus* _pPADStatus)
{
	if(!joysticks[_numPAD].enabled)
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
	int deadzone = ((float)(128.00/100.00)) * (float)(joysticks[_numPAD].deadzone+1);
	int deadzone2 = ((float)(-128.00/100.00)) * (float)(joysticks[_numPAD].deadzone+1);

	// Adjust range
	// The value returned by SDL_JoystickGetAxis is a signed integer (-32768 to 32768)
	// The value used for the gamecube controller is an unsigned char (0 to 255)
	int main_stick_x = (joystate[_numPAD].axis[CTL_MAIN_X]>>8);
	int main_stick_y = -(joystate[_numPAD].axis[CTL_MAIN_Y]>>8);
    int sub_stick_x = (joystate[_numPAD].axis[CTL_SUB_X]>>8);
	int sub_stick_y = -(joystate[_numPAD].axis[CTL_SUB_Y]>>8);

	// Quick fix
	if(main_stick_x > 127)
		main_stick_x = 127;
	if(main_stick_y > 127)
		main_stick_y = 127;
	if(sub_stick_x > 127)
		sub_stick_x = 127;
	if(sub_stick_y > 127)
		sub_stick_y = 127;

	if(main_stick_x < -128)
		main_stick_x = -128;
	if(main_stick_y < -128)
		main_stick_y = -128;
	if(sub_stick_x < -128)
		sub_stick_x = -128;
	if(sub_stick_y < -128)
		sub_stick_y = -128;

	// Send values to Dolpin
	if ((main_stick_x < deadzone2)	|| (main_stick_x > deadzone))	_pPADStatus->stickX += main_stick_x;
	if ((main_stick_y < deadzone2)	|| (main_stick_y > deadzone))	_pPADStatus->stickY += main_stick_y;
	if ((sub_stick_x < deadzone2)	|| (sub_stick_x > deadzone))	_pPADStatus->substickX += sub_stick_x;
	if ((sub_stick_y < deadzone2)	|| (sub_stick_y > deadzone))	_pPADStatus->substickY += sub_stick_y;

	// Set buttons
	if (joystate[_numPAD].buttons[CTL_L_SHOULDER])
	{
		_pPADStatus->button|=PAD_TRIGGER_L;
		_pPADStatus->triggerLeft  = 255;	// Perhaps support halfpress/pressure
	}
	if (joystate[_numPAD].buttons[CTL_R_SHOULDER])	
	{
		_pPADStatus->button|=PAD_TRIGGER_R;
		_pPADStatus->triggerRight = 255;	// Perhaps support halfpress/pressure
	}
	if (joystate[_numPAD].buttons[CTL_A_BUTTON])
	{
		_pPADStatus->button|=PAD_BUTTON_A;
		_pPADStatus->analogA = 255;			// Perhaps support halfpress/pressure
	}
	if (joystate[_numPAD].buttons[CTL_B_BUTTON])
	{
		_pPADStatus->button|=PAD_BUTTON_B;
		_pPADStatus->analogB = 255;			// Perhaps support halfpress/pressure
	}
	if (joystate[_numPAD].buttons[CTL_X_BUTTON])	_pPADStatus->button|=PAD_BUTTON_X;
	if (joystate[_numPAD].buttons[CTL_Y_BUTTON])	_pPADStatus->button|=PAD_BUTTON_Y;
	if (joystate[_numPAD].buttons[CTL_Z_TRIGGER])	_pPADStatus->button|=PAD_TRIGGER_Z;
	if (joystate[_numPAD].buttons[CTL_START])		_pPADStatus->button|=PAD_BUTTON_START;

	// Set D-pad
	if(joystate[_numPAD].dpad == SDL_HAT_LEFTUP		|| joystate[_numPAD].dpad == SDL_HAT_UP		|| joystate[_numPAD].dpad == SDL_HAT_RIGHTUP )		_pPADStatus->button|=PAD_BUTTON_UP;
	if(joystate[_numPAD].dpad == SDL_HAT_LEFTUP		|| joystate[_numPAD].dpad == SDL_HAT_LEFT	|| joystate[_numPAD].dpad == SDL_HAT_LEFTDOWN )		_pPADStatus->button|=PAD_BUTTON_LEFT;
	if(joystate[_numPAD].dpad == SDL_HAT_LEFTDOWN	|| joystate[_numPAD].dpad == SDL_HAT_DOWN	|| joystate[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_DOWN;
	if(joystate[_numPAD].dpad == SDL_HAT_RIGHTUP	|| joystate[_numPAD].dpad == SDL_HAT_RIGHT	|| joystate[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_RIGHT;

	_pPADStatus->err = PAD_ERR_NONE;
}

// Set PAD rumble
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Rumble(BYTE _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	// not supported by SDL
}

// Set PAD attached pads
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;
	
	LoadConfig();

	if(joysticks[0].enabled)
		connected |= 1;		
	if(joysticks[1].enabled)
		connected |= 2;
	if(joysticks[2].enabled)
		connected |= 4;
	if(joysticks[3].enabled)
		connected |= 8;

	return connected;
}

// Savestates
// ¯¯¯¯¯¯¯¯¯¯
unsigned int SaveLoadState(char *ptr, BOOL save)
{
	// not used
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Custom Functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Request joystick state
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void GetJoyState(int controller)
{
	SDL_JoystickUpdate();

	joystate[controller].axis[CTL_MAIN_X] = SDL_JoystickGetAxis(joystate[controller].joy, joysticks[controller].axis[CTL_MAIN_X]);
	joystate[controller].axis[CTL_MAIN_Y] = SDL_JoystickGetAxis(joystate[controller].joy, joysticks[controller].axis[CTL_MAIN_Y]);
	joystate[controller].axis[CTL_SUB_X] = SDL_JoystickGetAxis(joystate[controller].joy, joysticks[controller].axis[CTL_SUB_X]);
	joystate[controller].axis[CTL_SUB_Y] = SDL_JoystickGetAxis(joystate[controller].joy, joysticks[controller].axis[CTL_SUB_Y]);

	joystate[controller].buttons[CTL_L_SHOULDER] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].buttons[CTL_L_SHOULDER]);
	joystate[controller].buttons[CTL_R_SHOULDER] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].buttons[CTL_R_SHOULDER]);
	joystate[controller].buttons[CTL_A_BUTTON] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].buttons[CTL_A_BUTTON]);
	joystate[controller].buttons[CTL_B_BUTTON] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].buttons[CTL_B_BUTTON]);
	joystate[controller].buttons[CTL_X_BUTTON] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].buttons[CTL_X_BUTTON]);
	joystate[controller].buttons[CTL_Y_BUTTON] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].buttons[CTL_Y_BUTTON]);
	joystate[controller].buttons[CTL_Z_TRIGGER] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].buttons[CTL_Z_TRIGGER]);
	joystate[controller].buttons[CTL_START] = SDL_JoystickGetButton(joystate[controller].joy, joysticks[controller].buttons[CTL_START]);

	joystate[controller].dpad = SDL_JoystickGetHat(joystate[controller].joy, joysticks[controller].dpad);
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

	if(numjoy == 0)
	{
		MessageBox(NULL, "No Joystick detected!", NULL,  MB_ICONWARNING);
		return 0;
	}

	if(joyinfo)
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
		if(SDL_JoystickOpened(i))
			SDL_JoystickClose(joyinfo[i].joy);
	}

	return numjoy;
}

// Enable output log
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DEBUG_INIT()
{
	if(pFile)
		return;

	char dateStr [9]; 
	_strdate( dateStr);
	char timeStr [9]; 
	_strtime( timeStr );

	pFile = fopen ("nJoy-debug.txt","wt");
	fprintf(pFile, "nJoy v"INPUT_VERSION" Debug\n");
	fprintf(pFile, "Date: %s\nTime: %s\n", dateStr, timeStr);
	fprintf(pFile, "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\n");
}

// Disable output log
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DEBUG_QUIT()
{
	if(!pFile)
		return;

	char timeStr [9]; 
	_strtime(timeStr);

	fprintf(pFile, "_______________\n");
	fprintf(pFile, "Time: %s", timeStr);
	fclose(pFile);
}

// Save settings to file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void SaveConfig()
{
	IniFile file;
	file.Load("nJoy");

	for (int i=0; i<4; i++)
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
		file.Set(SectionName, "main_x", joysticks[i].axis[CTL_MAIN_X]);
		file.Set(SectionName, "main_y", joysticks[i].axis[CTL_MAIN_Y]);
		file.Set(SectionName, "sub_x", joysticks[i].axis[CTL_SUB_X]);
		file.Set(SectionName, "sub_y", joysticks[i].axis[CTL_SUB_Y]);
		file.Set(SectionName, "enabled", joysticks[i].enabled);
		file.Set(SectionName, "deadzone", joysticks[i].deadzone);
		file.Set(SectionName, "halfpress", joysticks[i].halfpress);
		file.Set(SectionName, "joy_id", joysticks[i].ID);
	}

	file.Save("nJoy");
}

// Load settings from file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void LoadConfig()
{
	IniFile file;
	file.Load("nJoy");

	for (int i=0; i<4; i++)
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
		file.Get(SectionName, "main_x", &joysticks[i].axis[CTL_MAIN_X], 0);	
		file.Get(SectionName, "main_y", &joysticks[i].axis[CTL_MAIN_Y], 1);	
		file.Get(SectionName, "sub_x", &joysticks[i].axis[CTL_SUB_X], 2);	
		file.Get(SectionName, "sub_y", &joysticks[i].axis[CTL_SUB_Y], 3);	
		file.Get(SectionName, "enabled", &joysticks[i].enabled, 1);	
		file.Get(SectionName, "deadzone", &joysticks[i].deadzone, 9);	
		file.Get(SectionName, "halfpress", &joysticks[i].halfpress, 0);	
		file.Get(SectionName, "joy_id", &joysticks[i].ID, 0);
	}
}
