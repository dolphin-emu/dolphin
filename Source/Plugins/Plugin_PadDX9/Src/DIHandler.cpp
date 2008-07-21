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

#include "stdafx.h" 
#include <math.h>

#include "PluginSpecs_Pad.h"
#include "multidi.h"
#include "DIHandler.h" 

/////////////////////////////////////////////////////////////////////////////////////////////////////
// T Y P E D E F S / D E F I N E S //////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// Actions used by this app
DIACTION CDIHandler::m_rgGameAction[] =
{
	// Keyboard input mappings
	{ CDIHandler::INPUT_MAIN_LEFT,		DIKEYBOARD_LEFT,    0, TEXT("Main left"),	},
	{ CDIHandler::INPUT_MAIN_RIGHT,		DIKEYBOARD_RIGHT,   0, TEXT("Main right"),	},
	{ CDIHandler::INPUT_MAIN_UP,		DIKEYBOARD_UP,		0, TEXT("Main up"),		},
	{ CDIHandler::INPUT_MAIN_DOWN,		DIKEYBOARD_DOWN,    0, TEXT("Main down"),	},
	{ CDIHandler::INPUT_CPAD_LEFT,		DIKEYBOARD_J,		0, TEXT("CPad left"),	},
	{ CDIHandler::INPUT_CPAD_RIGHT,		DIKEYBOARD_L,		0, TEXT("CPad right"),	},
	{ CDIHandler::INPUT_CPAD_UP,		DIKEYBOARD_I,		0, TEXT("CPad up"),		},
	{ CDIHandler::INPUT_CPAD_DOWN,		DIKEYBOARD_K,		0, TEXT("CPad down"),	},
	{ CDIHandler::INPUT_DPAD_LEFT,		DIKEYBOARD_F,		0, TEXT("DPad left"),	},
	{ CDIHandler::INPUT_DPAD_RIGHT,		DIKEYBOARD_H,		0, TEXT("DPad right"),	},
	{ CDIHandler::INPUT_DPAD_UP,		DIKEYBOARD_T,		0, TEXT("DPad up"),		},
	{ CDIHandler::INPUT_DPAD_DOWN,		DIKEYBOARD_G,		0, TEXT("DPad down"),	},
	{ CDIHandler::INPUT_BUTTON_START,	DIKEYBOARD_RETURN,	0, TEXT("Start"),		},
	{ CDIHandler::INPUT_BUTTON_A,		DIKEYBOARD_X,		0, TEXT("A-Button"),	},
	{ CDIHandler::INPUT_BUTTON_B,		DIKEYBOARD_Y,		0, TEXT("B-Button"),	},
	{ CDIHandler::INPUT_BUTTON_X,		DIKEYBOARD_S,		0, TEXT("X-Button"),	},
	{ CDIHandler::INPUT_BUTTON_Y,		DIKEYBOARD_C,		0, TEXT("Y-Button"),	},
	{ CDIHandler::INPUT_BUTTON_Z,		DIKEYBOARD_D,		0, TEXT("Z-Button"),	},
	{ CDIHandler::INPUT_BUTTON_L,		DIKEYBOARD_Q,		0, TEXT("L-Trigger"),	},
	{ CDIHandler::INPUT_BUTTON_R,		DIKEYBOARD_W,		0, TEXT("R-Trigger"),	},

	// Joystick input mappings
	{ CDIHandler::INPUT_MAIN_AXIS_LR,	DIAXIS_ARCADEP_LATERAL,	0, _T("Main left/right"),	},
	{ CDIHandler::INPUT_MAIN_AXIS_UD,	DIAXIS_ARCADEP_MOVE,	0, _T("Main Up/Down"),		},
	{ CDIHandler::INPUT_CPAD_AXIS_LR,	DIAXIS_ANY_1,			0, _T("CPad left/right"),	},
	{ CDIHandler::INPUT_CPAD_AXIS_UP,	DIAXIS_ANY_2,			0, _T("CPad Up/Down"),		},
	{ CDIHandler::INPUT_DPAD_AXIS_LR,	DIPOV_ANY_1,			0, _T("DPad left/right"),	},
	{ CDIHandler::INPUT_DPAD_AXIS_UP,	DIPOV_ANY_2,			0, _T("DPad Up/Down"),		},
};

const GUID g_guidApp = { 0x3afabad0, 0xd2c0, 0x4514, { 0xb4, 0x7e, 0x65, 0xfe, 0xf9, 0xb5, 0x14, 0x2e } };
#define SAMPLE_KEY        TEXT("Emulator\\Dolphin\\PadPlugin2")
#define NUMBER_OF_GAMEACTIONS    (sizeof(m_rgGameAction)/sizeof(DIACTION))

/////////////////////////////////////////////////////////////////////////////////////////////////////
// I M P L E M E N T A T I O N ////////////////////////// ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// ________________________________________________________________________________________ __________
// constructor
//
CDIHandler::CDIHandler(void) :
	m_hWnd(NULL),
	m_pInputDeviceManager(NULL)
{
}

// ________________________________________________________________________________________ __________
// destructor
//
CDIHandler::~CDIHandler(void)
{
	CleanupDirectInput();
}

// ________________________________________________________________________________________ __________
// InitInput
//
HRESULT 
CDIHandler::InitInput(HWND _hWnd)
{
	if (m_hWnd == _hWnd)
		return S_OK;

	CleanupDirectInput();

	m_hWnd = _hWnd;

	HRESULT hr;

	// Setup action format for the actual gameplay
	ZeroMemory( &m_diafGame, sizeof(DIACTIONFORMAT) );
	m_diafGame.dwSize          = sizeof(DIACTIONFORMAT);
	m_diafGame.dwActionSize    = sizeof(DIACTION);
	m_diafGame.dwDataSize      = NUMBER_OF_GAMEACTIONS * sizeof(DWORD);
	m_diafGame.guidActionMap   = g_guidApp;
	m_diafGame.dwGenre         = DIVIRTUAL_ARCADE_PLATFORM; 
	m_diafGame.dwNumActions    = NUMBER_OF_GAMEACTIONS;
	m_diafGame.rgoAction       = m_rgGameAction;
	m_diafGame.lAxisMin        = -100;
	m_diafGame.lAxisMax        = 100;
	m_diafGame.dwBufferSize    = 32;
	_tcscpy_s( m_diafGame.tszActionMap, _T("Dolphin Pad Plugin") );

	// Create a new input device manager
	m_pInputDeviceManager = new CMultiplayerInputDeviceManager( SAMPLE_KEY );

	if( FAILED( hr = ChangeNumPlayers( 1, FALSE, FALSE ) ) )
	{
		MessageBox(NULL, "InitInput", "Pad", MB_OK);
		return S_FALSE;
	}

	return S_OK;
}

// ________________________________________________________________________________________ __________
// ConfigInput
//
void 
CDIHandler::ConfigInput(void)
{
	HRESULT hr;

	CleanupDeviceStateStructs();

	// Configure the devices (with edit capability)
	hr = m_pInputDeviceManager->ConfigureDevices( m_hWnd, NULL, NULL, DICD_EDIT, NULL );    
	if( FAILED(hr) )
	{
		if( hr == E_DIUTILERR_PLAYERWITHOUTDEVICE )
		{
			// There's a player that hasn't been assigned a device.  Some games may
			// want to handle this by reducing the number of players, or auto-assigning
			// a device, or warning the user, etc.
			MessageBox( m_hWnd, TEXT("There is at least one player that wasn't assigned ") \
				TEXT("a device\n") \
				TEXT("Press OK to auto-assign a device to these users"), 
				TEXT("Player Without Device"), MB_OK | MB_ICONEXCLAMATION );
		}

		// Auto-reassign every player a device.
		ChangeNumPlayers( m_dwNumPlayers, FALSE, FALSE );
	}
}

// ________________________________________________________________________________________ __________
// CleanupDirectInput
//
void 
CDIHandler::CleanupDirectInput(void)
{
	if( NULL == m_pInputDeviceManager )
		return;

	CleanupDeviceStateStructs();

	// Cleanup DirectX input objects
	SAFE_DELETE( m_pInputDeviceManager );
}

// ________________________________________________________________________________________ __________
// CleanupDeviceStateStructs
//
void 
CDIHandler::CleanupDeviceStateStructs(void)
{
	// Get access to the list of semantically-mapped input devices
	// to delete all InputDeviceState structs before calling ConfigureDevices()
	CMultiplayerInputDeviceManager::DeviceInfo* pDeviceInfos;
	DWORD dwNumDevices;

	m_pInputDeviceManager->GetDevices( &pDeviceInfos, &dwNumDevices );

	for( DWORD i=0; i<dwNumDevices; i++ )
	{
		SUserInput* pInputDeviceState = (SUserInput*) pDeviceInfos[i].pParam;
		SAFE_DELETE( pInputDeviceState );
		pDeviceInfos[i].pParam = NULL;
	}
}

// ________________________________________________________________________________________ __________
// CleanupDeviceStateStructs
//
HRESULT 
CDIHandler::ChangeNumPlayers( DWORD dwNumPlayers, BOOL bResetOwnership, BOOL bResetMappings )
{
	HRESULT hr;

	m_dwNumPlayers = dwNumPlayers;

	// Just pass in stock names.  Real games may want to ask the user for a name, etc.
	TCHAR* strUserNames[4] = {	TEXT("Controller 1"), TEXT("Controller 2"), 
		TEXT("Controller 3"), TEXT("Controller 4") };

	BOOL bSuccess = FALSE;
	while( !bSuccess )
	{
		hr = m_pInputDeviceManager->Create( m_hWnd, strUserNames, m_dwNumPlayers, &m_diafGame, 
			StaticInputAddDeviceCB, NULL, 
			bResetOwnership, bResetMappings );

		if( FAILED(hr) )
		{
			switch( hr )
			{
			case E_DIUTILERR_DEVICESTAKEN:
				{
					// It's possible that a single user could "own" too many devices for the other
					// players to get into the game. If so, we reinit the manager class to provide 
					// each user with a device that has a default configuration.
					bResetOwnership = TRUE;

					MessageBox( m_hWnd, TEXT("You have entered more users than there are suitable ")       \
						TEXT("devices, or some users are claiming too many devices.\n") \
						TEXT("Press OK to give each user a default device"), 
						TEXT("Devices Are Taken"), MB_OK | MB_ICONEXCLAMATION );
					break;
				}

			case E_DIUTILERR_TOOMANYUSERS:
				{
					// Another common error is if more users are attempting to play than there are devices
					// attached to the machine. In this case, the number of players is automatically 
					// lowered to make playing the game possible. 
					DWORD dwNumDevices = m_pInputDeviceManager->GetNumDevices();
					m_dwNumPlayers = dwNumDevices;                    

					TCHAR sz[256];
					wsprintf( sz, TEXT("There are not enough devices attached to the ")          \
						TEXT("system for the number of users you entered.\nThe ")      \
						TEXT("number of users has been automatically changed to ")     \
						TEXT("%i (the number of devices available on the system)."),
						m_dwNumPlayers );
					MessageBox( m_hWnd, sz, _T("Too Many Users"), MB_OK | MB_ICONEXCLAMATION );                    
					break;
				}
			default:
				MessageBox(NULL, "Error creating DirectInput device.", "Pad DX9", MB_OK);
				return S_FALSE;
			}

			m_pInputDeviceManager->Cleanup();                                
		}
		else
		{
			bSuccess = TRUE;
		}
	}

	return S_OK;
}

// ________________________________________________________________________________________ __________
// UpdateInput
//
void 
CDIHandler::UpdateInput(void)
{
	if( NULL == m_pInputDeviceManager )
		return;

	CMultiplayerInputDeviceManager::DeviceInfo* pDeviceInfos;
	DWORD dwNumDevices;

	// Get access to the list of semantically-mapped input devices
	m_pInputDeviceManager->GetDevices( &pDeviceInfos, &dwNumDevices );

	// Loop through all devices and check game input
	for( DWORD i=0; i<dwNumDevices; i++ )
	{
		// skip past any devices that aren't assigned, 
		// since we don't care about them
		if( pDeviceInfos[i].pPlayerInfo == NULL )
			continue;

		DIDEVICEOBJECTDATA rgdod[10];
		DWORD dwItems = 10;
		HRESULT hr;
		LPDIRECTINPUTDEVICE8 pdidDevice = pDeviceInfos[i].pdidDevice;
		SUserInput* pInputDeviceState = (SUserInput*) pDeviceInfos[i].pParam;

		// dunno if we need this
		if(pDeviceInfos[i].bRelativeAxis)
		{
			// For relative axis data, the action mapper only informs us when
			// the delta data is non-zero, so we need to zero its state
			// out each frame
			pInputDeviceState->fMainLR = 0.0f;
			pInputDeviceState->fMainUP = 0.0f;
			pInputDeviceState->fCPadLR = 0.0f;
			pInputDeviceState->fCPadUP = 0.0f;
			pInputDeviceState->fDPadLR = 0.0f;
			pInputDeviceState->fDPadUP = 0.0f;
			pInputDeviceState->fTriggerL = 0.0f;
			pInputDeviceState->fTriggerR = 0.0f;
		}

		hr = pdidDevice->Acquire();
		hr = pdidDevice->Poll();
		hr = pdidDevice->GetDeviceData( sizeof(DIDEVICEOBJECTDATA), rgdod, &dwItems, 0 );
		if( FAILED(hr) )
			continue;

		// Get the sematics codes for the game menu
		for( DWORD j=0; j<dwItems; j++ )
		{
			bool  bButtonState = (rgdod[j].dwData==0x80) ? true : false;
			FLOAT fAxisState   = (FLOAT)((int)rgdod[j].dwData)/100.0f;

			switch( rgdod[j].uAppData )
			{
				// Handle relative axis data
			case INPUT_MAIN_AXIS_LR:	pInputDeviceState->fMainLR = fAxisState;	break;
			case INPUT_MAIN_AXIS_UD:	pInputDeviceState->fMainUP = fAxisState;	break;
			case INPUT_CPAD_AXIS_LR:	pInputDeviceState->fCPadLR = fAxisState;	break;
			case INPUT_CPAD_AXIS_UP:	pInputDeviceState->fCPadUP = fAxisState;	break;
			case INPUT_DPAD_AXIS_LR:	pInputDeviceState->fDPadLR = fAxisState;	break;
			case INPUT_DPAD_AXIS_UP:	pInputDeviceState->fDPadUP = fAxisState;	break;

				// Handle buttons separately so the button state data
				// doesn't overwrite the axis state data, and handle
				// each button separately so they don't overwrite each other
			case INPUT_MAIN_LEFT:		pInputDeviceState->bMainLeft	= bButtonState; break;
			case INPUT_MAIN_RIGHT:		pInputDeviceState->bMainRight	= bButtonState; break;
			case INPUT_MAIN_UP:			pInputDeviceState->bMainUp		= bButtonState; break;
			case INPUT_MAIN_DOWN:		pInputDeviceState->bMainDown	= bButtonState; break;
			case INPUT_CPAD_LEFT:		pInputDeviceState->bCPadLeft	= bButtonState; break;
			case INPUT_CPAD_RIGHT:		pInputDeviceState->bCPadRight	= bButtonState; break;
			case INPUT_CPAD_UP:			pInputDeviceState->bCPadUp		= bButtonState; break;
			case INPUT_CPAD_DOWN:		pInputDeviceState->bCPadDown	= bButtonState; break;
			case INPUT_DPAD_LEFT:		pInputDeviceState->bDPadLeft	= bButtonState; break;
			case INPUT_DPAD_RIGHT:		pInputDeviceState->bDPadRight	= bButtonState; break;
			case INPUT_DPAD_UP:			pInputDeviceState->bDPadUp		= bButtonState; break;
			case INPUT_DPAD_DOWN:		pInputDeviceState->bDPadDown	= bButtonState; break;

			case INPUT_BUTTON_START:	pInputDeviceState->bButtonStart	= bButtonState; break;
			case INPUT_BUTTON_A:		pInputDeviceState->bButtonA		= bButtonState; break;
			case INPUT_BUTTON_B:		pInputDeviceState->bButtonB		= bButtonState; break;
			case INPUT_BUTTON_X:		pInputDeviceState->bButtonX		= bButtonState; break;
			case INPUT_BUTTON_Y:		pInputDeviceState->bButtonY		= bButtonState; break;
			case INPUT_BUTTON_Z:		pInputDeviceState->bButtonZ		= bButtonState; break;
			case INPUT_BUTTON_L:		pInputDeviceState->fTriggerL	= bButtonState; break;
			case INPUT_BUTTON_R:		pInputDeviceState->fTriggerR	= bButtonState; break;
			}
		}
	}

	for( DWORD iPlayer=0; iPlayer<m_dwNumPlayers; iPlayer++ )
	{       
		SControllerInput& ctrlInput = m_controllerInput[iPlayer];

		m_controllerInput[iPlayer].fMainLR = 0.0f;
		m_controllerInput[iPlayer].fMainUP = 0.0f;
		m_controllerInput[iPlayer].fCPadLR = 0.0f;
		m_controllerInput[iPlayer].fCPadUP = 0.0f;
		m_controllerInput[iPlayer].fDPadLR = 0.0f;
		m_controllerInput[iPlayer].fDPadUP = 0.0f;

		ctrlInput.bButtonA = false;
		ctrlInput.bButtonB = false;
		ctrlInput.bButtonX = false;
		ctrlInput.bButtonY = false;
		ctrlInput.bButtonZ = false;
		ctrlInput.bButtonStart = false;
		ctrlInput.fTriggerL = false;
		ctrlInput.fTriggerR = false;

		// Concatinate the data from all the DirectInput devices
		for(DWORD i=0; i<dwNumDevices; i++)
		{
			// Only look at devices that are assigned to this player 
			if( pDeviceInfos[i].pPlayerInfo == NULL || 
				pDeviceInfos[i].pPlayerInfo->dwPlayerIndex != iPlayer )
				continue;

			SUserInput* pInputDeviceState = (SUserInput*)pDeviceInfos[i].pParam;			

			// main-axis
			if( fabs(pInputDeviceState->fMainLR) > fabs(ctrlInput.fMainLR) )
				ctrlInput.fMainLR = pInputDeviceState->fMainLR;
			if( fabs(pInputDeviceState->fMainUP) > fabs(ctrlInput.fMainUP) )
				ctrlInput.fMainUP = pInputDeviceState->fMainUP;

			if (pInputDeviceState->bMainLeft)	ctrlInput.fMainLR = -1.0f;
			if (pInputDeviceState->bMainRight)	ctrlInput.fMainLR = 1.0f;
			if (pInputDeviceState->bMainUp)		ctrlInput.fMainUP = -1.0f;
			if (pInputDeviceState->bMainDown)	ctrlInput.fMainUP = 1.0f;

			// CPad-axis
			if( fabs(pInputDeviceState->fCPadLR) > fabs(ctrlInput.fCPadLR) )
				ctrlInput.fCPadLR = pInputDeviceState->fCPadLR;
			if( fabs(pInputDeviceState->fCPadUP) > fabs(ctrlInput.fCPadUP) )
				ctrlInput.fCPadUP = pInputDeviceState->fCPadUP;

			if (pInputDeviceState->bCPadLeft)	ctrlInput.fCPadLR = -1.0f;
			if (pInputDeviceState->bCPadRight)	ctrlInput.fCPadLR = 1.0f;
			if (pInputDeviceState->bCPadUp)		ctrlInput.fCPadUP = -1.0f;
			if (pInputDeviceState->bCPadDown)	ctrlInput.fCPadUP = 1.0f;

			// DPad-axis
			if( fabs(pInputDeviceState->fDPadLR) > fabs(ctrlInput.fDPadLR) )
				ctrlInput.fDPadLR = pInputDeviceState->fDPadLR;
			if( fabs(pInputDeviceState->fDPadUP) > fabs(ctrlInput.fDPadUP) )
				ctrlInput.fDPadUP = pInputDeviceState->fDPadUP;

			if (pInputDeviceState->bDPadLeft)	ctrlInput.fDPadLR = -1.0f;
			if (pInputDeviceState->bDPadRight)	ctrlInput.fDPadLR = 1.0f;
			if (pInputDeviceState->bDPadUp)		ctrlInput.fDPadUP = -1.0f;
			if (pInputDeviceState->bDPadDown)	ctrlInput.fDPadUP = 1.0f;

			// buttons
			if (pInputDeviceState->bButtonA)	ctrlInput.bButtonA = true;
			if (pInputDeviceState->bButtonB)	ctrlInput.bButtonB = true;
			if (pInputDeviceState->bButtonX)	ctrlInput.bButtonX = true;
			if (pInputDeviceState->bButtonY)	ctrlInput.bButtonY = true;
			if (pInputDeviceState->bButtonZ)	ctrlInput.bButtonZ = true;
			if (pInputDeviceState->bButtonStart)ctrlInput.bButtonStart = true;
			if (pInputDeviceState->fTriggerL)	ctrlInput.fTriggerL = true;
			if (pInputDeviceState->fTriggerR)	ctrlInput.fTriggerR = true;
		} 
	}
}

// ________________________________________________________________________________________ __________
// StaticInputAddDeviceCB
//
HRESULT CALLBACK 
CDIHandler::StaticInputAddDeviceCB(	CMultiplayerInputDeviceManager::PlayerInfo* pPlayerInfo, 
								   CMultiplayerInputDeviceManager::DeviceInfo* pDeviceInfo, 
								   const DIDEVICEINSTANCE* pdidi, 
								   LPVOID pParam)
{
	CDIHandler* pApp = (CDIHandler*) pParam;
	return pApp->InputAddDeviceCB( pPlayerInfo, pDeviceInfo, pdidi );
}

// ________________________________________________________________________________________ __________
// InputAddDeviceCB
//
HRESULT  
CDIHandler::InputAddDeviceCB(	CMultiplayerInputDeviceManager::PlayerInfo* pPlayerInfo, 
							 CMultiplayerInputDeviceManager::DeviceInfo* pDeviceInfo, 
							 const DIDEVICEINSTANCE* pdidi)
{
	if( (GET_DIDEVICE_TYPE(pdidi->dwDevType) != DI8DEVTYPE_KEYBOARD) &&
		(GET_DIDEVICE_TYPE(pdidi->dwDevType) != DI8DEVTYPE_MOUSE) )
	{
		// Setup the deadzone 
		DIPROPDWORD dipdw;
		dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
		dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dipdw.diph.dwObj        = 0;
		dipdw.diph.dwHow        = DIPH_DEVICE;
		dipdw.dwData            = 500;
		pDeviceInfo->pdidDevice->SetProperty( DIPROP_DEADZONE, &dipdw.diph );
	}

	// Create a new InputDeviceState for each device so the 
	// app can record its state 
	SUserInput* pNewInputDeviceState = new SUserInput;
	ZeroMemory( pNewInputDeviceState, sizeof(SUserInput) );
	pDeviceInfo->pParam = (LPVOID) pNewInputDeviceState;

	return S_OK;
}