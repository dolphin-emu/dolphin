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



////////////////////////////////////////////////////////////////////////////////////////
// Issues
/* ¯¯¯¯¯¯¯¯¯

   The StrangeHack in ConfigAdvanced.cpp doesn't work in Linux, it still wont resize the
   window correctly. So currently in Linux you have to have advanced controls enabled when
   you open the window to see them.

////////////////////////*/



////////////////////////////////////////////////////////////////////////////////////////
// Variables guide
/* ¯¯¯¯¯¯¯¯¯

   The arrays joysticks[] and joystate[] are numbered 0 to 3 for the four different virtual
   controllers. Joysticks[].ID will have the number of the inputs device mapped to that controller,
   this value can be between 0 and the total number of connected physical devices. The mapping
   is initially done by PAD_Initialize(), but for the configuration we can remap them, like in
   ConfigBox::ChangeJoystick().

   The joyinfo[] array is for a certain physical device. It's therefore used as
   joyinfo[joysticks[controller].ID].

////////////////////////*/



//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯
#include "nJoy.h"

// Declare config window so that we can write debugging info to it from functions in this file
#if defined(HAVE_WX) && HAVE_WX
	ConfigBox* m_frame;
#endif
/////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Variables
// ¯¯¯¯¯¯¯¯¯

// Rumble in windows
#define _CONTROLLER_STATE_H // Avoid certain declarations in nJoy.h
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

	// Start the pads so we can use them in the configuration and advanced controls
	if(!emulator_running)
	{
		SPADInitialize _PADInitialize;
		_PADInitialize.hWnd = NULL;
		_PADInitialize.pLog = NULL;
		PAD_Initialize(_PADInitialize);
		emulator_running = FALSE; // Set it back to false
	}

	g_Config.Load(); // Load settings

	// We don't need a parent for this wxDialog
	//wxWindow win;
	//win.SetHWND(_hParent);
	//ConfigBox frame(&win);
	//win.SetHWND(0);

	m_frame = new ConfigBox(NULL);
	m_frame->ShowModal();	

	#else
	if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
	{
		printf("Could not initialize SDL! (%s)\n", SDL_GetError());
		return;
	}

	g_Config.Load();	// load settings

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
	
	if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
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

	Search_Devices(); // Populate joyinfo for all attached devices
	g_Config.Load();	// Load joystick mapping, joysticks[].ID etc
	if (joysticks[0].enabled)
		joystate[0].joy = SDL_JoystickOpen(joysticks[0].ID);
	if (joysticks[1].enabled)
		joystate[1].joy = SDL_JoystickOpen(joysticks[1].ID);
	if (joysticks[2].enabled)
		joystate[2].joy = SDL_JoystickOpen(joysticks[2].ID);
	if (joysticks[3].enabled)
		joystate[3].joy = SDL_JoystickOpen(joysticks[3].ID);
}


// Search attached devices. Populate joyinfo for all attached physical devices.
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

	// Warn the user if no joysticks are detected
	if (numjoy == 0)
	{		
		#ifdef _WIN32
		//MessageBox(NULL, "No Joystick detected!", NULL,  MB_ICONWARNING);
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
		// Open the device to be able to read the values
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

		// We have now read the values we need so we close the device
		if (SDL_JoystickOpened(i)) SDL_JoystickClose(joyinfo[i].joy);
	}

	return numjoy;
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


// Set buttons status from wxWidgets in the main application
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Input(u8 _Key, u8 _UpDown)
{
	// Check if the keys are interesting, and then update it
	for(int i = 0; i < 4; i++)
	{
		for(int j = CTL_L_SHOULDER; j <= CTL_START; j++)
		{
			if (joysticks[i].buttons[j] == _Key)
				{ joystate[i].buttons[j] = _UpDown; break; }
		}
	}
}


// Set PAD status. This is called from SerialInterface_Devices.cpp
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	if (!joysticks[_numPAD].enabled)
		return;
		
	// Clear pad status
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	// Get pad status
	GetJoyState(_numPAD);
	

	///////////////////////////////////////////////////
	// Set analog controllers
	// -----------

	// Read values
	int i_main_stick_x = joystate[_numPAD].axis[CTL_MAIN_X];
	int i_main_stick_y = -joystate[_numPAD].axis[CTL_MAIN_Y];
    int i_sub_stick_x = joystate[_numPAD].axis[CTL_SUB_X];
	int i_sub_stick_y = -joystate[_numPAD].axis[CTL_SUB_Y];

	// Check if we should make adjustments
	if(g_Config.bSquareToCircle.at(_numPAD))
	{
		std::vector<int> main_xy = Pad_Square_to_Circle(i_main_stick_x, i_main_stick_y);
		i_main_stick_x = main_xy.at(0);
		i_main_stick_y = main_xy.at(1);
	}

	// Convert button values from 0xffff to 0xff
	u8 main_stick_x = Pad_Convert(i_main_stick_x);
	u8 main_stick_y = Pad_Convert(i_main_stick_y);
    u8 sub_stick_x = Pad_Convert(i_sub_stick_x);
	u8 sub_stick_y = Pad_Convert(i_sub_stick_y);

	// Set Deadzones perhaps out of function
	int deadzone = (int)(((float)(128.00/100.00)) * (float)(joysticks[_numPAD].deadzone + 1));
	int deadzone2 = (int)(((float)(-128.00/100.00)) * (float)(joysticks[_numPAD].deadzone + 1));

	// Send values to Dolpin if they are outside the deadzone
	if ((main_stick_x < deadzone2)	|| (main_stick_x > deadzone))	_pPADStatus->stickX = main_stick_x;
	if ((main_stick_y < deadzone2)	|| (main_stick_y > deadzone))	_pPADStatus->stickY = main_stick_y;
	if ((sub_stick_x < deadzone2)	|| (sub_stick_x > deadzone))	_pPADStatus->substickX = sub_stick_x;
	if ((sub_stick_y < deadzone2)	|| (sub_stick_y > deadzone))	_pPADStatus->substickY = sub_stick_y;


	///////////////////////////////////////////////////
	// Set buttons
	// -----------

	// The L and R trigger values
	int triggervalue = 255;
	if (joystate[_numPAD].halfpress) triggervalue = 100;

	// Neutral button value, no button pressed
	_pPADStatus->button |= PAD_USE_ORIGIN;

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
			_pPADStatus->button |= PAD_BUTTON_UP;
		if (joystate[_numPAD].dpad2[CTL_D_PAD_DOWN])
			_pPADStatus->button |= PAD_BUTTON_DOWN;
		if (joystate[_numPAD].dpad2[CTL_D_PAD_LEFT])
			_pPADStatus->button |= PAD_BUTTON_LEFT;
		if (joystate[_numPAD].dpad2[CTL_D_PAD_RIGHT])
			_pPADStatus->button |= PAD_BUTTON_RIGHT;
	}
	
	//
	_pPADStatus->err = PAD_ERR_NONE;

	// Use rumble
	Pad_Use_Rumble(_numPAD, _pPADStatus);
}


//////////////////////////////////////////////////////////////////////////////////////////
// Convert stick values, for example from circle to square analog stick radius
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

/* Convert stick values.

   The value returned by SDL_JoystickGetAxis is a signed integer s16
   (-32768 to 32767). The value used for the gamecube controller is an unsigned 
   char u8 (0 to 255) with neutral at 0x80 (128), so that it's equivalent to a signed
   -128 to 127. */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
u8 Pad_Convert(int _val)
{
	/* If the limits on joystate[].axis[] actually is a u16 then we don't need this
	   but if it's not actually limited to that we need to apply these limits */
	if(_val > 32767) _val = 32767; // upper limit
	if(_val < -32768) _val = -32768; // lower limit

	// Convert (-32768 to 32767) to (-128 to 127)
	_val = _val >> 8;

	// Convert (-128 to 127) to (0 to 255)
	u8 val = 0x80 + _val;

	return val;
}


/* Convert the stick raidus from a circular to a square. I don't know what input values
   the actual GC controller produce for the GC, it may be a square, a circle or something
   in between. But one thing that is certain is that PC pads differ in their output (as
   shown in the list below), so it may be beneficiary to convert whatever radius they
   produce to the radius the GC games expect. This is the first implementation of this
   that convert a square radius to a circual radius. Use the advanced settings to enable
   and calibrate it.

   Observed diagonals:
   Perfect circle: 71% = sin(45)
   Logitech Dual Action: 100%
   Dual Shock 2 (Original) with Super Dual Box Pro: 90%
   XBox 360 Wireless: 85%
   GameCube Controller (Third Party) with EMS TrioLinker Plus II: 60%
*/
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
float SquareDistance(float deg)
{
	// See if we have to adjust the angle
	deg = abs(deg);
	if( (deg > 45 && deg < 135) ) deg = deg - 90;

	float rad = deg * M_PI / 180;

	float val = abs(cos(rad));
	float dist = 1 / val; // Calculate distance from center

	//m_frame->m_pStatusBar2->SetLabel(wxString::Format("Deg:%f  Val:%f  Dist:%f", deg, val, dist));

	return dist;
}
std::vector<int> Pad_Square_to_Circle(int _x, int _y)
{
	/* Do we need this? */
	if(_x > 32767) _x = 32767; if(_y > 32767) _y = 32767; // upper limit
	if(_x < -32768) _x = -32768; if(_y > 32767) _y = 32767; // lower limit

	///////////////////////////////////////////////////
	// Convert to circle
	// -----------
	int Tmp = atoi (g_Config.SDiagonal.substr(0, g_Config.SDiagonal.length() - 1).c_str());
	float Diagonal = Tmp / 100.0;

	// First make a perfect square in case we don't have one already
	float OrigDist = sqrt(  pow((float)_y, 2) + pow((float)_x, 2)  ); // Get current distance
	float rad = atan2((float)_y, (float)_x); // Get current angle
	float deg = rad * 180 / M_PI;

	// A diagonal of 85% means a distance of 1.20
	float corner_circle_dist = ( Diagonal / sin(45 * M_PI / 180) );
	float SquareDist = SquareDistance(deg);
	float adj_ratio1; // The original-to-square distance adjustment
	float adj_ratio2 = SquareDist; // The circle-to-square distance adjustment
	float final_ratio; // The final adjustment to the current distance
	float result_dist; // The resulting distance

	// Calculate the corner-to-square adjustment ratio
	if(corner_circle_dist < SquareDist) adj_ratio1 = SquareDist / corner_circle_dist;
		else adj_ratio1 = 1;

	// Calculate the resulting distance
	result_dist = OrigDist * adj_ratio1 / adj_ratio2;

	float x = result_dist * cos(rad); // calculate x
	float y = result_dist * sin(rad); // calculate y

	int int_x = (int)floor(x);
	int int_y = (int)floor(y);

	// Debugging
	//m_frame->m_pStatusBar2->SetLabel(wxString::Format("%f  %f  %i", corner_circle_dist, Diagonal, Tmp));

	std::vector<int> vec;
	vec.push_back(int_x);
	vec.push_back(int_y);
	return vec;
}
//////////////////////////////////////////////////////////////////////////////////////////


// Set PAD attached pads
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;
	
	g_Config.Load();

	if (joysticks[0].enabled) connected |= 1;		
	if (joysticks[1].enabled) connected |= 2;
	if (joysticks[2].enabled) connected |= 4;
	if (joysticks[3].enabled) connected |= 8;

	return connected;
}



//////////////////////////////////////////////////////////////////////////////////////////
// Read current joystick status
/* ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
	The value joysticks[].buttons[] is the number of the assigned joypad button,
	joystate[].buttons[] is the status of the button, it becomes 0 (no pressed) or 1 (pressed) */

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

// Request joystick state. The input value "controller" is for a virtual controller 0 to 3.
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
//////////////////////////////////////////////////////////////////////////////////////////
