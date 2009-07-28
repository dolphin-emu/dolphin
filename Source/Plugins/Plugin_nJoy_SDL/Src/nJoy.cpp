//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003 Dolphin Project.
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
   // TODO : we should not need a Hack in the first place :/

////////////////////////*/

 

////////////////////////////////////////////////////////////////////////////////////////
// Variables guide
/* ¯¯¯¯¯¯¯¯¯

   Joyinfo[1, 2, 3, 4, ..., number of attached devices]: Gamepad info that is populate by Search_Devices()
   PadMapping[1, 2, 3 and 4]: The button mapping
   Joystate[1, 2, 3 and 4]: The current button states

   The arrays PadMapping[] and PadState[] are numbered 0 to 3 for the four different virtual
   controllers. Joysticks[].ID will have the number of the physical input device mapped to that
   controller (this value range between 0 and the total number of connected physical devices). The
   mapping of a certain physical device to PadState[].joy is initially done by Initialize(), but
   for the configuration we can remap that, like in PADConfigDialognJoy::ChangeJoystick().

   The joyinfo[] array holds the physical gamepad info for a certain physical device. It's therefore
   used as joyinfo[PadMapping[controller].ID] if we want to get the joyinfo for a certain joystick.

////////////////////////*/

 

//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯
#include "nJoy.h"
#include "LogManager.h"

// Declare config window so that we can write debugging info to it from functions in this file
#if defined(HAVE_WX) && HAVE_WX
	PADConfigDialognJoy* m_ConfigFrame = NULL;
#endif
/////////////////////////

 
//////////////////////////////////////////////////////////////////////////////////////////
// Variables
// ¯¯¯¯¯¯¯¯¯

#define _EXCLUDE_MAIN_ // Avoid certain declarations in nJoy.h
FILE *pFile;
std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
InputCommon::CONTROLLER_STATE PadState[4];
InputCommon::CONTROLLER_MAPPING PadMapping[4];
bool g_EmulatorRunning = false;
int NumPads = 0, NumGoodPads = 0, LastPad = 0;
#ifdef _WIN32
	HWND m_hWnd = NULL, m_hConsole = NULL; // Handle to window
#endif
SPADInitialize *g_PADInitialize = NULL;
PLUGIN_GLOBALS* globals = NULL;

// Rumble 
#if defined(__linux__)
	extern int fd;
#endif

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

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager *)globals->logManager);
}


// Call config dialog
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DllConfig(HWND _hParent)
{
#ifdef _WIN32
	// Start the pads so we can use them in the configuration and advanced controls
	if (!g_EmulatorRunning)
	{
		Search_Devices(joyinfo, NumPads, NumGoodPads); // Populate joyinfo for all attached devices

		// Check if a DirectInput error occured
		if (ReloadDLL())
		{
			PostMessage(_hParent, WM_USER, NJOY_RELOAD, 0);
			return;
		}
	}
#else
	if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
	{
		printf("Could not initialize SDL! (%s)\n", SDL_GetError());
		return;
	}
#endif

	g_Config.Load();	// load settings

#if defined(HAVE_WX) && HAVE_WX
	if (!m_ConfigFrame)
		m_ConfigFrame = new PADConfigDialognJoy(GetParentedWxWindow(_hParent));
	else if (!m_ConfigFrame->GetParent()->IsShown())
		m_ConfigFrame->Close(true);

	// Only allow one open at a time
	if (!m_ConfigFrame->IsShown())
		m_ConfigFrame->ShowModal();
	else
		m_ConfigFrame->Hide();
#endif
}

void DllDebugger(HWND _hParent, bool Show) {}

 
// Init PAD (start emulation)
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
/* Information: This function can not be run twice without a Shutdown in between. If
   it's run twice the SDL_Init() will cause a crash. One solution to this is to keep a
   global function that remembers the SDL_Init() and SDL_Quit() (g_EmulatorRunning does
   not do that since we can open and close this without any game running). But I would
   suggest that avoiding to run this twice from the Core is better. */
void Initialize(void *init)
{
	// Debugging
	//	#ifdef SHOW_PAD_STATUS
	//		Console::Open(110);
	//		m_hConsole = Console::GetHwnd();
	//	#endif
	INFO_LOG(CONSOLE, "Initialize: %i\n", SDL_WasInit(0));
    g_PADInitialize = (SPADInitialize*)init;
	g_EmulatorRunning = true;
	
	#ifdef _WIN32
		m_hWnd = (HWND)g_PADInitialize->hWnd;
	#endif

	#ifdef _DEBUG
		DEBUG_INIT();
	#endif

	// Populate joyinfo for all attached devices if the configuration window is not already open
#if defined(HAVE_WX) && HAVE_WX
	if(!m_ConfigFrame)
	{
		Search_Devices(joyinfo, NumPads, NumGoodPads);
		// Check if a DirectInput error occured
		if(ReloadDLL()) g_PADInitialize->padNumber = -1;
	}
#endif
	#ifdef RERECORDING
		Recording::Initialize();
	#endif
}

// Shutdown PAD (stop emulation)
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
/* Information: This function can not be run twice without an Initialize in between. If
   it's run twice the SDL_...() functions below will cause a crash.
   Called from: The Dolphin Core, PADConfigDialognJoy::OnClose() */
void Shutdown()
{
	INFO_LOG(CONSOLE, "Shutdown: %i\n", SDL_WasInit(0));

	// -------------------------------------------
	// Play back input instead of accepting any user input
	// ----------------------
	#ifdef RERECORDING
		Recording::ShutDown();
	#endif
	// ----------------------

	// Always change this variable
	g_EmulatorRunning = false;

	// Stop debugging
	#ifdef _DEBUG
		DEBUG_QUIT();
	#endif
	
	#ifdef _WIN32
		// Free DInput before closing SDL, or get a crash !
		FreeDirectInput();
	#elif defined(__linux__)
		close(fd);
	#endif

	// Don't shutdown the gamepad if the configuration window is still showing
	// Todo: Coordinate with the Wiimote plugin, SDL_Quit() will remove the pad for it to
#if defined(HAVE_WX) && HAVE_WX
	if (m_ConfigFrame) return;
#endif

	/* Close all devices carefully. We must check that we are not accessing any undefined
	   vector elements or any bad devices */
	for (int i = 0; i < 4; i++)
	{
		if (joyinfo.size() > (u32)PadMapping[i].ID)
			if (joyinfo.at(PadMapping[i].ID).Good)
				if(SDL_JoystickOpened(PadMapping[i].ID))
				{
					SDL_JoystickClose(PadState[i].joy);
					PadState[i].joy = NULL;
				}
	}

	// Clear the physical device info
	joyinfo.clear();
	NumPads = 0;
	NumGoodPads = 0;

	// Finally close SDL
	if (SDL_WasInit(0)) SDL_Quit();

	// Remove the pointer to the initialize data
	g_PADInitialize = NULL;
}


// Set buttons status from keyboard input. Currently this is done from wxWidgets in the main application.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Input(u16 _Key, u8 _UpDown)
{
	// Check that Dolphin is in focus, otherwise don't update the pad status
	if (!IsFocus()) return;

	// Check if the keys are interesting, and then update it
	for(int i = 0; i < 4; i++)
	{
		for(int j = InputCommon::CTL_L_SHOULDER; j <= InputCommon::CTL_START; j++)
		{
			if (PadMapping[i].buttons[j] == _Key)
				{ PadState[i].buttons[j] = _UpDown; break; }
		}

		for(int j = InputCommon::CTL_D_PAD_UP; j <= InputCommon::CTL_D_PAD_RIGHT; j++)
		{
			if (PadMapping[i].dpad2[j] == _Key)
				{ PadState[i].dpad2[j] = _UpDown; break; }
		}
	}

	// Debugging
	//INFO_LOG(CONSOLE, "%i", _Key);
}


// Save state
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DoState(unsigned char **ptr, int mode)
{
#ifdef RERECORDING
	Recording::DoState(ptr, mode);
#endif
}


// Set PAD status
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Called from: SI_DeviceGCController.cpp
// Function: Gives the current pad status to the Core
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	//INFO_LOG(CONSOLE, "PAD_GetStatus(): %i %i %i\n", _numPAD, PadMapping[_numPAD].enabled, PadState[_numPAD].joy);

	/* Check if the pad is avaliable, currently we don't disable pads just because they are
	   disconnected */
	if (!PadState[_numPAD].joy) return;

	// -------------------------------------------
	// Play back input instead of accepting any user input
	// ----------------------
	#ifdef RERECORDING
	if (g_Config.bPlayback)
	{
		*_pPADStatus = Recording::Play();
		return;
	}
	#endif

	// ----------------------

	// Clear pad status
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	// Check that Dolphin is in focus, otherwise don't update the pad status
	if (g_Config.bCheckFocus || IsFocus())
		GetJoyState(PadState[_numPAD], PadMapping[_numPAD], _numPAD, joyinfo[PadMapping[_numPAD].ID].NumButtons);

	// Get type
	int TriggerType = PadMapping[_numPAD].triggertype;
	int TriggerValue = PadState[_numPAD].halfpress ? 100 : 255;
 
	///////////////////////////////////////////////////
	// The analog controls
	// -----------

	// Read axis values
	int i_main_stick_x = PadState[_numPAD].axis[InputCommon::CTL_MAIN_X];
	int i_main_stick_y = -PadState[_numPAD].axis[InputCommon::CTL_MAIN_Y];
    int i_sub_stick_x = PadState[_numPAD].axis[InputCommon::CTL_SUB_X];
	int i_sub_stick_y = -PadState[_numPAD].axis[InputCommon::CTL_SUB_Y];
	int TriggerLeft = PadState[_numPAD].axis[InputCommon::CTL_L_SHOULDER];
	int TriggerRight = PadState[_numPAD].axis[InputCommon::CTL_R_SHOULDER];

	// Check if we should make adjustments
	if (PadMapping[_numPAD].bSquareToCircle) InputCommon::Square2Circle(i_main_stick_x, i_main_stick_y, _numPAD, PadMapping[_numPAD].SDiagonal);
	// Radius adjustment
	if (PadMapping[_numPAD].bRadiusOnOff) InputCommon::RadiusAdjustment(i_main_stick_x, i_main_stick_y, _numPAD, PadMapping[_numPAD].SRadius);
	// C-stick
	if (PadMapping[_numPAD].bSquareToCircleC) InputCommon::Square2Circle(i_sub_stick_x, i_sub_stick_y, _numPAD, PadMapping[_numPAD].SDiagonalC);
	if (PadMapping[_numPAD].bRadiusOnOffC) InputCommon::RadiusAdjustment(i_sub_stick_x, i_sub_stick_y, _numPAD, PadMapping[_numPAD].SRadiusC);

	// Convert axis values
	u8 main_stick_x = InputCommon::Pad_Convert(i_main_stick_x);
	u8 main_stick_y = InputCommon::Pad_Convert(i_main_stick_y);
    u8 sub_stick_x = InputCommon::Pad_Convert(i_sub_stick_x);
	u8 sub_stick_y = InputCommon::Pad_Convert(i_sub_stick_y);

	// Convert the triggers values, if we are using analog triggers at all
	if (PadMapping[_numPAD].triggertype == InputCommon::CTL_TRIGGER_SDL)
	{
		if (PadMapping[_numPAD].buttons[InputCommon::CTL_L_SHOULDER] >= 1000) TriggerLeft = InputCommon::Pad_Convert(TriggerLeft);
		if (PadMapping[_numPAD].buttons[InputCommon::CTL_R_SHOULDER] >= 1000) TriggerRight = InputCommon::Pad_Convert(TriggerRight);
	}

	// Set Deadzone
	float deadzone = (128.00 * (float)(PadMapping[_numPAD].deadzone + 1.00)) / 100.00;
	float distance_main = (float)sqrt((float)(main_stick_x * main_stick_x) + (float)(main_stick_y * main_stick_y));
	float distance_sub = (float)sqrt((float)(sub_stick_x * sub_stick_x) + (float)(sub_stick_y * sub_stick_y));

	// Send values to Dolpin if they are outside the deadzone
	if (distance_main > deadzone)
	{
		_pPADStatus->stickX = main_stick_x;
		_pPADStatus->stickY = main_stick_y;
	}
	if (distance_sub > deadzone)
	{
		_pPADStatus->substickX = sub_stick_x;
		_pPADStatus->substickY = sub_stick_y;
	}
 
	///////////////////////////////////////////////////
	// The L and R triggers
	// -----------

	// Neutral value, no button pressed
	_pPADStatus->button |= PAD_USE_ORIGIN;	

	// Check if the digital L button is pressed
	if (PadState[_numPAD].buttons[InputCommon::CTL_L_SHOULDER])
	{
		if (!PadState[_numPAD].halfpress)
			_pPADStatus->button |= PAD_TRIGGER_L;
		_pPADStatus->triggerLeft = TriggerValue;
	}
	// no the digital L button is not pressed, but the analog left trigger is
	else if (TriggerLeft > 0)
		_pPADStatus->triggerLeft = TriggerLeft;

	// Check if the digital R button is pressed
	if (PadState[_numPAD].buttons[InputCommon::CTL_R_SHOULDER])
	{
		if (!PadState[_numPAD].halfpress)
			_pPADStatus->button |= PAD_TRIGGER_R;
		_pPADStatus->triggerRight = TriggerValue;
	}
	// no the digital R button is not pressed, but the analog right trigger is
	else if (TriggerRight > 0)
		_pPADStatus->triggerRight = TriggerRight;

	// Update the buttons in analog mode too
	if (TriggerLeft > 0xf0) _pPADStatus->button |= PAD_TRIGGER_L;
	if (TriggerRight > 0xf0) _pPADStatus->button |= PAD_TRIGGER_R;


	///////////////////////////////////////////////////
	// The digital buttons
	// -----------	
	if (PadState[_numPAD].buttons[InputCommon::CTL_A_BUTTON])
	{
		_pPADStatus->button |= PAD_BUTTON_A;
		_pPADStatus->analogA = 255;			// Perhaps support pressure?
	}
	if (PadState[_numPAD].buttons[InputCommon::CTL_B_BUTTON])
	{
		_pPADStatus->button |= PAD_BUTTON_B;
		_pPADStatus->analogB = 255;			// Perhaps support pressure?
	}
	if (PadState[_numPAD].buttons[InputCommon::CTL_X_BUTTON])	_pPADStatus->button|=PAD_BUTTON_X;
	if (PadState[_numPAD].buttons[InputCommon::CTL_Y_BUTTON])	_pPADStatus->button|=PAD_BUTTON_Y;
	if (PadState[_numPAD].buttons[InputCommon::CTL_Z_TRIGGER])	_pPADStatus->button|=PAD_TRIGGER_Z;
	if (PadState[_numPAD].buttons[InputCommon::CTL_START])		_pPADStatus->button|=PAD_BUTTON_START;


	///////////////////////////////////////////////////
	// The D-pad
	// -----------		
	if (PadMapping[_numPAD].controllertype == InputCommon::CTL_DPAD_HAT)
	{
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTUP	|| PadState[_numPAD].dpad == SDL_HAT_UP		|| PadState[_numPAD].dpad == SDL_HAT_RIGHTUP )		_pPADStatus->button|=PAD_BUTTON_UP;
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTUP	|| PadState[_numPAD].dpad == SDL_HAT_LEFT	|| PadState[_numPAD].dpad == SDL_HAT_LEFTDOWN )		_pPADStatus->button|=PAD_BUTTON_LEFT;
		if (PadState[_numPAD].dpad == SDL_HAT_LEFTDOWN	|| PadState[_numPAD].dpad == SDL_HAT_DOWN	|| PadState[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_DOWN;
		if (PadState[_numPAD].dpad == SDL_HAT_RIGHTUP	|| PadState[_numPAD].dpad == SDL_HAT_RIGHT	|| PadState[_numPAD].dpad == SDL_HAT_RIGHTDOWN )	_pPADStatus->button|=PAD_BUTTON_RIGHT;
	}
	else
	{
		if (PadState[_numPAD].dpad2[InputCommon::CTL_D_PAD_UP])
			_pPADStatus->button |= PAD_BUTTON_UP;
		if (PadState[_numPAD].dpad2[InputCommon::CTL_D_PAD_DOWN])
			_pPADStatus->button |= PAD_BUTTON_DOWN;
		if (PadState[_numPAD].dpad2[InputCommon::CTL_D_PAD_LEFT])
			_pPADStatus->button |= PAD_BUTTON_LEFT;
		if (PadState[_numPAD].dpad2[InputCommon::CTL_D_PAD_RIGHT])
			_pPADStatus->button |= PAD_BUTTON_RIGHT;
	}

	// Update error code
	_pPADStatus->err = PAD_ERR_NONE;

	// -------------------------------------------
	// Rerecording
	// ----------------------
	#ifdef RERECORDING
	// Record input
	if (g_Config.bRecording) Recording::RecordInput(*_pPADStatus);
	#endif
	// ----------------------

	// Debugging 
	/*	
	// Show the status of all connected pads
	ConsoleListener* Console = LogManager::GetInstance()->getConsoleListener();
	if ((LastPad == 0 && _numPAD == 0) || _numPAD < LastPad) Console->ClearScreen();	
	LastPad = _numPAD;
//	Console->ClearScreen();
	int X = _pPADStatus->stickX - 128, Y = _pPADStatus->stickY - 128;
	int Xc = _pPADStatus->substickX - 128, Yc = _pPADStatus->substickY - 128;
	NOTICE_LOG(CONSOLE, 
		"Pad        | Number:%i Enabled:%i Handle:%i\n"
		"Stick      | X:%03i Y:%03i R:%3.0f\n"
		"C-Stick    | X:%03i Y:%03i R:%3.0f\n"
		"Trigger    | StatusL:%04x StatusR:%04x  TriggerL:%04x TriggerR:%04x  TriggerValue:%i\n"
		"Buttons    | Overall:%i  A:%i X:%i\n"
		"======================================================\n",

		_numPAD, PadMapping[_numPAD].enabled, PadState[_numPAD].joy,

		X, Y, sqrt((float)(X*X + Y*Y)),
		Xc, Yc, sqrt((float)(Xc*Xc + Yc*Yc)),

		_pPADStatus->triggerLeft, _pPADStatus->triggerRight,  TriggerLeft, TriggerRight,  TriggerValue,

		_pPADStatus->button,
		PadState[_numPAD].buttons[InputCommon::CTL_A_BUTTON],
		PadState[_numPAD].buttons[InputCommon::CTL_X_BUTTON]
		);
	*/
}


///////////////////////////////////////////////// Spec functions


//******************************************************************************
// Supporting functions
//******************************************************************************


//////////////////////////////////////////////////////////////////////////////////////////
// Search for SDL devices
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
bool Search_Devices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads, int &_NumGoodPads)
{
	bool Success = InputCommon::SearchDevices(_joyinfo, _NumPads, _NumGoodPads);

	// Warn the user if no gamepads are detected
	if (_NumGoodPads == 0 && g_EmulatorRunning)
	{
		PanicAlert("nJoy: No Gamepad Detected");
		return false;
	}

	// Load PadMapping[] etc
	g_Config.Load();

	// Update the PadState[].joy handle
	for (int i = 0; i < 4; i++)
	{
		if (joyinfo.size() > (u32)PadMapping[i].ID)
			if(joyinfo.at(PadMapping[i].ID).Good)
				PadState[i].joy = SDL_JoystickOpen(PadMapping[i].ID);
	}

	return Success;
}


//////////////////////////////////////////////////////////////////////////////////////////
/* Check if any of the pads failed to open. In Windows there is a strange "IDirectInputDevice2::
   SetDataFormat() DirectX error -2147024809" after exactly four SDL_Init() and SDL_Quit() */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
bool ReloadDLL()
{
	if (   (PadState[0].joy == NULL)
		|| (PadState[1].joy == NULL)
		|| (PadState[2].joy == NULL)
		|| (PadState[3].joy == NULL))
	{
		// Check if it was an error and not just no pads connected
		std::string StrError = SDL_GetError();
		if (StrError.find("IDirectInputDevice2") != std::string::npos)
		{
			// Clear the physical device info
			joyinfo.clear();
			NumPads = 0;
			NumGoodPads = 0;
			// Close SDL
			if (SDL_WasInit(0)) SDL_Quit();
			// Log message
			INFO_LOG(CONSOLE, "Error: %s\n", StrError.c_str());	
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Check if Dolphin is in focus
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
bool IsFocus()
{
return true;

#ifdef _WIN32
	HWND RenderingWindow = NULL; if (g_PADInitialize) RenderingWindow = g_PADInitialize->hWnd;
	HWND Parent = GetParent(RenderingWindow);
	HWND TopLevel = GetParent(Parent);
	HWND Config = NULL; if (m_ConfigFrame) Config = (HWND)m_ConfigFrame->GetHWND();
	// Support both rendering to main window and not, and the config and eventual console window
	if (GetForegroundWindow() == TopLevel || GetForegroundWindow() == RenderingWindow || GetForegroundWindow() == Config || GetForegroundWindow() == m_hConsole)
		return true;
	else
		return false;
#else
	return true;
#endif
}




 

