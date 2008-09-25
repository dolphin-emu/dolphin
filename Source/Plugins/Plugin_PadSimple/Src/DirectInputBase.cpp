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

#include "stdafx.h"
#include "DirectInputBase.h"

DInput::DInput()
	: g_pDI(NULL),
	g_pKeyboard(NULL)
{}


DInput::~DInput()
{
	Free();
}

void DInput::DIKToString(unsigned int keycode, char *keyStr)
{
	switch(keycode) {
		case DIK_RETURN:
			sprintf(keyStr, "Enter");
			break;
		case DIK_UP:
			sprintf(keyStr, "Up");
			break;
		case DIK_DOWN:
			sprintf(keyStr, "Down");
			break;
		case DIK_LEFT:
			sprintf(keyStr, "Left");
			break;
		case DIK_RIGHT:
			sprintf(keyStr, "Right");
			break;
		case DIK_HOME:		 
			strcpy(keyStr, "Home");		 
			break;		 
		case DIK_END:		 
			strcpy(keyStr, "End");		 
			break;		 
		case DIK_INSERT:		 
			strcpy(keyStr, "Ins");		 
			break;		 
		case DIK_DELETE:		 
			strcpy(keyStr, "Del");		 
			break;		 
		case DIK_PGUP:		 
			strcpy(keyStr, "PgUp");		 
			break;		 
		case DIK_PGDN:		 
			strcpy(keyStr, "PgDn");		 
			break;		 
		default:		 
			GetKeyNameText(keycode << 16, keyStr, 64);		 
			break;
	}
}

HRESULT DInput::Init(HWND hWnd)
{
	HRESULT hr;
	DWORD dwCoopFlags;
	dwCoopFlags = DISCL_FOREGROUND | DISCL_NOWINKEY;

	// Create a DInput object
	if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
					    IID_IDirectInput8, (VOID* *)&g_pDI, NULL)))
	{
		MessageBox(0, "Direct Input Create Failed", 0, MB_ICONERROR);
		return(hr);
	}

	if (FAILED(hr = g_pDI->CreateDevice(GUID_SysKeyboard, &g_pKeyboard, NULL)))
	{
		MessageBox(0, "Couldn't access keyboard", 0, MB_ICONERROR);
		Free();
		return(hr);
	}

	g_pKeyboard->SetDataFormat(&c_dfDIKeyboard);
	g_pKeyboard->SetCooperativeLevel(hWnd, dwCoopFlags);
	g_pKeyboard->Acquire();

	return(S_OK);
}

void DInput::Free()
{
	if (g_pKeyboard)
	{
		g_pKeyboard->Unacquire();
		g_pKeyboard->Release();
		g_pKeyboard = 0;
	}

	if (g_pDI)
	{
		g_pDI->Release();
		g_pDI = 0;
	}
}

// Desc: Read the input device's state when in immediate mode and display it.
HRESULT DInput::Read()
{
	HRESULT hr;

	if (NULL == g_pKeyboard)
	{
		return(S_OK);
	}

	// Get the input's device state, and put the state in dims
	ZeroMemory(diks, sizeof(diks));
	hr = g_pKeyboard->GetDeviceState(sizeof(diks), diks);

	//for (int i=0; i<256; i++)
	//	if (diks[i])MessageBox(0,"DSFJDKSF|",0,0);
	if (FAILED(hr))
	{
		// DirectInput may be telling us that the input stream has been
		// interrupted.  We aren't tracking any state between polls, so
		// we don't have any special reset that needs to be done.
		// We just re-acquire and try again.

		// If input is lost then acquire and keep trying
		hr = g_pKeyboard->Acquire();

		while (hr == DIERR_INPUTLOST)
		{
			hr = g_pKeyboard->Acquire();
		}

		// hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
		// may occur when the app is minimized or in the process of
		// switching, so just try again later
		return(S_OK);
	}

	return(S_OK);
}
