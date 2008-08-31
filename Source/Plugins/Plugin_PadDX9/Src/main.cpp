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
#include "resource.h"
#include "PluginSpecs_Pad.h"

#include "IniFile.h"
#include "AboutDlg.h"
#include "ConfigDlg.h"

#include "MultiDI.h"
#include "DIHandler.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
// T Y P E D E F S / D E F I N E S //////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

HINSTANCE g_hInstance = NULL;
SPADInitialize g_PADInitialize;

CDIHandler g_diHandler;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// I M P L E M E N T A T I O N ////////////////////////// ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// ________________________________________________________________________________________ __________
// DllMain
//
BOOL APIENTRY DllMain(	HINSTANCE _hinstDLL,	// DLL module handle
						DWORD dwReason,		// reason called
						LPVOID _lpvReserved)	// reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;

	case DLL_PROCESS_DETACH:
		break;

	default:
		break;
	}

	g_hInstance = _hinstDLL;
	return TRUE;
}

// __________________________________________________________________________________________________
// GetDllInfo
//
void GetDllInfo (PLUGIN_INFO* _PluginInfo) 
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

#ifndef _DEBUG
	sprintf_s(_PluginInfo->Name, 100, "Pad DirectX9");
#else
	sprintf_s(_PluginInfo->Name, 100, "Pad DirectX9 (Debug)");
#endif
}

// __________________________________________________________________________________________________
// DllAbout
//
void DllAbout(HWND _hParent) 
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal(_hParent);
}

// __________________________________________________________________________________________________
// DllConfig
//
void DllConfig(HWND _hParent)
{
	g_diHandler.InitInput(_hParent);

	EnableWindow(_hParent, FALSE);
	g_diHandler.ConfigInput();
	EnableWindow(_hParent, TRUE);
	SetForegroundWindow(_hParent);

	g_diHandler.CleanupDirectInput();
}

// __________________________________________________________________________________________________
// PAD_Initialize
//
void PAD_Initialize(SPADInitialize _PADInitialize)
{
	g_PADInitialize = _PADInitialize;
	g_diHandler.InitInput((HWND)_PADInitialize.hWnd);
}

// __________________________________________________________________________________________________
// PAD_Shutdown
//
void PAD_Shutdown(void) 
{
	g_diHandler.CleanupDirectInput();
}

// __________________________________________________________________________________________________
// PADGetStatus
//
void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	// check if all is okay
	if ((_numPAD != 0) || // we support just pad 0
		(_pPADStatus == NULL))
		return;

#ifdef RECORD_REPLAY
	*_pPADStatus = PlayRecord();
	return;
#endif

	int base = 0x80;	

	// clear pad
	memset(_pPADStatus,0,sizeof(SPADStatus));

	_pPADStatus->stickY = base;
	_pPADStatus->stickX = base;
	_pPADStatus->substickX = base;
	_pPADStatus->substickY = base;

	_pPADStatus->button |= PAD_USE_ORIGIN;
	// just update pad on focus
	if (g_PADInitialize.hWnd != ::GetForegroundWindow())
		return;

	//get keys from dinput
	g_diHandler.UpdateInput();

	const SControllerInput& rInput = g_diHandler.GetControllerInput(_numPAD);

//	int mainvalue =    (dinput.diks[keyForControl[CTL_HALFMAIN]]   &0xFF) ? 40 : 100;
//	int subvalue =     (dinput.diks[keyForControl[CTL_HALFSUB]]    &0xFF) ? 40 : 100;
//	int triggervalue = (dinput.diks[keyForControl[CTL_HALFTRIGGER]]&0xFF) ? 100 : 255;

	// get the new keys
	if (rInput.bButtonStart)	_pPADStatus->button		|= PAD_BUTTON_START;
	if (rInput.bButtonA)		{_pPADStatus->button	|= PAD_BUTTON_A; _pPADStatus->analogA = 255;}
	if (rInput.bButtonB)		{_pPADStatus->button	|= PAD_BUTTON_B; _pPADStatus->analogB = 255;}
	if (rInput.bButtonX)		_pPADStatus->button		|= PAD_BUTTON_X;
	if (rInput.bButtonY)		_pPADStatus->button		|= PAD_BUTTON_Y;
	if (rInput.bButtonZ)		_pPADStatus->button		|= PAD_TRIGGER_Z;	

	if (rInput.fDPadLR < 0)		_pPADStatus->button		|= PAD_BUTTON_UP;
	if (rInput.fDPadLR > 0)		_pPADStatus->button		|= PAD_BUTTON_DOWN;
	if (rInput.fDPadUP < 0)		_pPADStatus->button		|= PAD_BUTTON_LEFT;
	if (rInput.fDPadUP > 0)		_pPADStatus->button		|= PAD_BUTTON_RIGHT;

	if (rInput.fTriggerL > 0)	{_pPADStatus->button	|= PAD_TRIGGER_L; _pPADStatus->triggerLeft  = 255;}
	if (rInput.fTriggerR > 0)	{_pPADStatus->button	|= PAD_TRIGGER_R; _pPADStatus->triggerRight = 255;}

	_pPADStatus->stickX		=	0x80 + (unsigned __int8)(rInput.fMainLR * 127.f);
	_pPADStatus->stickY		=	0x80 + (unsigned __int8)(rInput.fMainUP * -127.f);
	_pPADStatus->substickX	=	0x80 + (unsigned __int8)(rInput.fCPadLR * 127.f);
	_pPADStatus->substickY	=	0x80 + (unsigned __int8)(rInput.fCPadUP * -127.f);

	_pPADStatus->err = PAD_ERR_NONE;
}

// __________________________________________________________________________________________________
// PAD_Rumble
//
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
}
