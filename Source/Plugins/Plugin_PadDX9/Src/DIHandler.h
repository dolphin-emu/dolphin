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
/////////////////////////////////////////////////////////////////////////////////////////////////////
// M O D U L E  B E G I N ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once


// Struct to store the current input state
struct SControllerInput
{
	float fMainLR;
	float fMainUP;
	float fCPadLR;
	float fCPadUP;
	float fDPadLR;
	float fDPadUP;

	bool  bButtonStart;
	bool  bButtonA;
	bool  bButtonB;
	bool  bButtonX;
	bool  bButtonY;
	bool  bButtonZ;

	float fTriggerL;
	float fTriggerR;
};

class CDIHandler
{
public:
	CDIHandler(void);

	~CDIHandler(void);

	HRESULT InitInput(HWND _hWnd);
	void ConfigInput(void);
	void CleanupDirectInput(void);
	void UpdateInput(void);

	const SControllerInput& GetControllerInput(unsigned int _number)
	{
		return m_controllerInput[_number];
	}

private:

	enum
	{
		MAX_AXIS = 127
	};

	enum INPUT_SEMANTICS
	{
		// Gameplay semantics
		INPUT_MAIN_AXIS_LR=1,  
		INPUT_MAIN_AXIS_UD,       
		INPUT_MAIN_LEFT,
		INPUT_MAIN_RIGHT,
		INPUT_MAIN_UP,
		INPUT_MAIN_DOWN,
		INPUT_CPAD_AXIS_LR,
		INPUT_CPAD_AXIS_UP,
		INPUT_CPAD_LEFT,
		INPUT_CPAD_RIGHT,
		INPUT_CPAD_UP,
		INPUT_CPAD_DOWN,
		INPUT_DPAD_AXIS_LR,
		INPUT_DPAD_AXIS_UP,
		INPUT_DPAD_LEFT,
		INPUT_DPAD_RIGHT,
		INPUT_DPAD_UP,
		INPUT_DPAD_DOWN,
		INPUT_BUTTON_START,
		INPUT_BUTTON_A,
		INPUT_BUTTON_B,
		INPUT_BUTTON_X,
		INPUT_BUTTON_Y,
		INPUT_BUTTON_Z,
		INPUT_BUTTON_L,
		INPUT_BUTTON_R
	};

	// Struct to store the current input state
	struct SUserInput
	{
		bool bMainLeft;
		bool bMainRight;
		bool bMainUp;
		bool bMainDown;
		float fMainLR;
		float fMainUP;

		bool bCPadLeft;
		bool bCPadRight;
		bool bCPadUp;
		bool bCPadDown;
		float fCPadLR;
		float fCPadUP;

		bool bDPadLeft;
		bool bDPadRight;
		bool bDPadUp;
		bool bDPadDown;
		float fDPadLR;
		float fDPadUP;

		bool  bButtonStart;
		bool  bButtonA;
		bool  bButtonB;
		bool  bButtonX;
		bool  bButtonY;
		bool  bButtonZ;

		bool fTriggerL;
		bool fTriggerR;
	};

	// handle to window that "owns" the DInput
	HWND m_hWnd;

	static DIACTION m_rgGameAction[];

	// DirectInput multiplayer device manager
	CMultiplayerInputDeviceManager* m_pInputDeviceManager;  

	// Action format for game play
	DIACTIONFORMAT m_diafGame;

	// Struct for storing user input 
	SControllerInput m_controllerInput[4];

	// Number of players in the game
	DWORD m_dwNumPlayers;

	void CleanupDeviceStateStructs(void);	

	HRESULT ChangeNumPlayers(DWORD _dwNumPlayers, BOOL _bResetOwnership, BOOL _bResetMappings);

	static HRESULT CALLBACK StaticInputAddDeviceCB(CMultiplayerInputDeviceManager::PlayerInfo* _pPlayerInfo, 
		CMultiplayerInputDeviceManager::DeviceInfo* _pDeviceInfo, 
		const DIDEVICEINSTANCE* _pdidi, 
		LPVOID _pParam);

	HRESULT InputAddDeviceCB(CMultiplayerInputDeviceManager::PlayerInfo* _pPlayerInfo, 
		CMultiplayerInputDeviceManager::DeviceInfo* _pDeviceInfo, 
		const DIDEVICEINSTANCE* _pdidi);	
};