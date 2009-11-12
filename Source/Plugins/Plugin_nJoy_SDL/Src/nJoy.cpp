
// Project description
// -------------------
// Name: nJoy
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003 Dolphin Project.
//

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

// Include
// ---------
#include "nJoy.h"
#include "LogManager.h"

// Declare config window so that we can write debugging info to it from functions in this file
#if defined(HAVE_WX) && HAVE_WX
	PADConfigDialognJoy* m_ConfigFrame = NULL;
#endif



// Variables
// ---------
#define _EXCLUDE_MAIN_ // Avoid certain declarations in nJoy.h
FILE *pFile;
std::vector<InputCommon::CONTROLLER_INFO> joyinfo;
InputCommon::CONTROLLER_STATE PadState[4];
InputCommon::CONTROLLER_MAPPING PadMapping[4];
bool g_EmulatorRunning = false;
bool SDLPolling = true;
int NumPads = 0, NumDIDevices = -1, LastPad = 0;
#ifdef _WIN32
	HWND m_hWnd = NULL, m_hConsole = NULL; // Handle to window
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
	sprintf(_PluginInfo->Name, "nJoy (DebugFast) by Falcon4ever");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "nJoy by Falcon4ever");
#else
	sprintf(_PluginInfo->Name, "nJoy (Debug) by Falcon4ever");
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
	// Populate joyinfo for all attached devices
	LocalSearchDevices(joyinfo, NumPads);
	//g_Config.Load();	// load settings

#if defined(HAVE_WX) && HAVE_WX
	if (!m_ConfigFrame)
		m_ConfigFrame = new PADConfigDialognJoy(GetParentedWxWindow(_hParent));
	else if (!m_ConfigFrame->GetParent()->IsShown())
		m_ConfigFrame->Close(true);

	// Only allow one open at a time
	if (!m_ConfigFrame->IsShown())
		m_ConfigFrame->DoShow();
	else
		m_ConfigFrame->Hide();	
#endif
}

void DllDebugger(HWND _hParent, bool Show) {}

 
// Init PAD (start emulation)
// --------------------------
void Initialize(void *init)
{
	INFO_LOG(PAD, "Initialize: %i\n", SDL_WasInit(0));
    g_PADInitialize = (SPADInitialize*)init;
	g_EmulatorRunning = true;
	
	#ifdef _WIN32
		m_hWnd = (HWND)g_PADInitialize->hWnd;
	#endif

	#ifdef _DEBUG
		DEBUG_INIT();
	#endif

	// Populate joyinfo for all attached devices
	LocalSearchDevices(joyinfo, NumPads);
}

// Shutdown PAD (stop emulation)
// -----------------------------
/* Information: This function can not be run twice without an Initialize in between. If
   it's run twice the SDL_...() functions below will cause a crash.
   Called from: The Dolphin Core, PADConfigDialognJoy::OnClose() */
void Shutdown()
{
	INFO_LOG(PAD, "Shutdown: %i\n", SDL_WasInit(0));

	// Always change this variable
	g_EmulatorRunning = false;

	// Stop debugging
	#ifdef _DEBUG
		DEBUG_QUIT();
	#endif

	PAD_RumbleClose();

	/* Close all devices carefully. We must check that we are not accessing any undefined
	   vector elements or any bad devices */
	for (int i = 0; i < 4; i++)
	{
		if (joyinfo.size() > (u32)PadMapping[i].ID)
			if(SDL_JoystickOpened(PadMapping[i].ID))
			{
				SDL_JoystickClose(PadState[i].joy);
				PadState[i].joy = NULL;
			}
	}

	// Clear the physical device info
	joyinfo.clear();
	NumPads = 0;

	// Finally close SDL
	SDL_Quit();

	// Remove the pointer to the initialize data
	g_PADInitialize = NULL;
}


// Set buttons status from keyboard input. Currently this is done from wxWidgets in the main application.
// --------------
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
}


// Save state
// --------------
void DoState(unsigned char **ptr, int mode)
{
#ifdef RERECORDING
	Recording::DoState(ptr, mode);
#endif
}


// Set PAD status
// --------------
// Called from: SI_DeviceGCController.cpp
// Function: Gives the current pad status to the Core
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	if (!IsPolling()) return;
	
	// Update joyinfo handles. This is in case the Wiimote plugin has restarted SDL after a pad was conencted/disconnected
	// so that the handles are updated.
	LocalSearchDevices(joyinfo, NumPads);
	
	// Check if the pad is avaliable, currently we don't disable pads just because they are
	// disconnected
	if (!PadState[_numPAD].joy) return;

	// Clear pad status
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	// Check that Dolphin is in focus, otherwise don't update the pad status
	if (g_Config.bCheckFocus || IsFocus())
		GetJoyState(PadState[_numPAD], PadMapping[_numPAD]);

	// Get type
	int TriggerType = PadMapping[_numPAD].triggertype;
	int TriggerValue = PadState[_numPAD].halfpress ? 100 : 255;
 
	
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
}


///////////////////////////////////////////////// Spec functions


//******************************************************************************
// Supporting functions
//******************************************************************************



// Search for SDL devices
// ----------------

bool LocalSearchDevices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads)
{
	// Turn off device polling while searching
	EnablePolling(false);

	//DEBUG_LOG(PAD, "LocalSearchDevices");
	bool Success = InputCommon::SearchDevices(_joyinfo, _NumPads);
	
	DoLocalSearchDevices(_joyinfo, _NumPads);
	
	EnablePolling(true);
	
	return Success;
}

bool LocalSearchDevicesReset(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads)
{
	//DEBUG_LOG(PAD, "LocalSearchDevicesUpdate: %i", IsPolling());
	
	bool Success = InputCommon::SearchDevicesReset(_joyinfo, _NumPads);
	
	DoLocalSearchDevices(_joyinfo, _NumPads);
	
	return Success;
}


bool DoLocalSearchDevices(std::vector<InputCommon::CONTROLLER_INFO> &_joyinfo, int &_NumPads)
{	
	// Don't warn the user if no gamepads are detected. If the pad doesn't respond he will open the configuration and fix it.
	// Also, if he's using a Wii game he may not care that the gamepad is turned off.
	if (_NumPads == 0 && g_EmulatorRunning)
	{
		//PanicAlert("nJoy: No Gamepad Detected");
		return false;
	}

	// Load the first time
	if (!g_Config.Loaded) g_Config.Load();

	// Update the PadState[].joy handle
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < joyinfo.size(); j++)
		{
			if (joyinfo.at(j).Name == PadMapping[i].Name)
			{
				PadState[i].joy = joyinfo.at(j).joy;
				//DEBUG_LOG(PAD, "Slot %i: %s", i, joyinfo.at(j).Name.c_str());
			}
		}
	}

	return true;
}


// Is the device connected?
// ----------------
bool IsConnected(std::string Name)
{
	for (int i = 0; i < joyinfo.size(); i++)
	{
		//DEBUG_LOG(PAD, "IDToName: ID %i id %i %s", ID, i, joyinfo.at(i).Name.c_str());
		if (joyinfo.at(i).Name == Name)
			return true;
	}
}

// Earlier versions of SDL could crash if SDL functions were called during SDL_Quit/SDL_Init. I could not get
// such a crash in the new version even with frequent SDL_Quit/SDL_Init so thse functions are disabled now.
// ----------------
bool IsPolling()
{
	return true;
	/*
	if (!SDLPolling || SDL_JoystickEventState(SDL_QUERY) == SDL_ENABLE)
		return false;
	else
		return true;
	*/
}
void EnablePolling(bool Enable)
{
	/*
	if (Enable)
	{
		SDLPolling = true;
		SDL_JoystickEventState(SDL_IGNORE);
	}
	else
	{
		SDLPolling = false;
		SDL_JoystickEventState(SDL_ENABLE);
	}
	*/
}

// ID to Name
// ----------------
std::string IDToName(int ID)
{
	for (int i = 0; i < joyinfo.size(); i++)
	{
		//DEBUG_LOG(PAD, "IDToName: ID %i id %i %s", ID, i, joyinfo.at(i).Name.c_str());
		if (joyinfo.at(i).ID == ID)
			return joyinfo.at(i).Name;
	}
	return "";
}

// ID to id - Get the gamepad id from a device ID (the ids will be all or some of the IDs)
// ----------------
int IDToid(int ID)
{
	for (int i = 0; i < joyinfo.size(); i++)
	{
		if (joyinfo.at(i).ID == ID)
			return i;
	}
	if (joyinfo.size() > 0)
	{
		ERROR_LOG(PAD, "IDToid error");
		return 0;
	}
	else
	{
		PanicAlert("Error in IDToid. The plugin may crash.");
		return -1;
	}
}


// Check if Dolphin is in focus
// ----------------
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




 

