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
	u32 ISOId;
	TLogv pLog;
	TWiimoteInput pWiimoteInput;
} SWiimoteInitialize;


// I N T E R F A C E 


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

#include "ExportEpilog.h"

#endif	//_WIIMOTE_H_INCLUDED__
