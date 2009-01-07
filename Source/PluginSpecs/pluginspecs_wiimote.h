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


//__________________________________________________________________________________________________
// Common wiimote plugin spec, unversioned
//

#ifndef _WIIMOTE_H_INCLUDED__
#define _WIIMOTE_H_INCLUDED__

#include "PluginSpecs.h"
#include "ExportProlog.h"


typedef void (*TLogv)(const char* _pMessage, int _v);

// This is called when the Wiimote sends input reports to the Core.
// Payload: an L2CAP packet.
typedef void (*TWiimoteInput)(u16 _channelID, const void* _pData, u32 _Size);

// This data is passed from the core on initialization.
typedef struct
{
	HWND hWnd;
	TLogv pLog;
	TWiimoteInput pWiimoteInput;
} SWiimoteInitialize;


/////////////////////////////////////////////////////////////////////////////////////////////////////
// I N T E R F A C E ////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// __________________________________________________________________________________________________
// Function: 
// Purpose:  
// input:    WiimoteInitialize
// output:   If at least one real Wiimote was found or not
//
EXPORT bool CALL Wiimote_Initialize(SWiimoteInitialize _WiimoteInitialize);

// __________________________________________________________________________________________________
// Function: Wiimote_Shutdown
// Purpose:  This function is called when the emulator is closing
//           down allowing the DLL to de-initialise.
// input:    none
// output:   none
//
EXPORT void CALL Wiimote_Shutdown();

// __________________________________________________________________________________________________
// Function: Wiimote_Output
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID CONTROL channel.
// input:    Da pakket.
// output:   none
//
EXPORT void CALL Wiimote_ControlChannel(u16 _channelID, const void* _pData, u32 _Size);

// __________________________________________________________________________________________________
// Function: Wiimote_Input
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID INTERRUPT channel.
// input:    Da pakket.
// output:   none
//
EXPORT void CALL Wiimote_InterruptChannel(u16 _channelID, const void* _pData, u32 _Size);

// __________________________________________________________________________________________________
// Function: Wiimote_Update
// Purpose:  This function is called periodically by the Core.
// input:    none
// output:   none
//
EXPORT void CALL Wiimote_Update();

// __________________________________________________________________________________________________
// Function: PAD_GetAttachedPads
// Purpose:  Get mask of attached pads (eg: controller 1 & 4 -> 0x9)
// input:	 none
// output:   number of pads
//
EXPORT unsigned int CALL Wiimote_GetAttachedControllers();

// __________________________________________________________________________________________________
// Function: Wiimote_DoState
// Purpose:  Saves/load state
// input/output: ptr
// input: mode
//
EXPORT void CALL Wiimote_DoState(void *ptr, int mode);

#include "ExportEpilog.h"

#endif	//_WIIMOTE_H_INCLUDED__
